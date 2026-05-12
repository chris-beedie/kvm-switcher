/*********************************************************************
 USB HID keyboard host — native ESP32-S3 USB-OTG -> UART link to RP2350

 Pipeline:
   USB keyboard -> ESP32-S3 USB-OTG host -> hid_host_keyboard_callback
     -> hotkey filter -> hidLinkSendKeyboard() -> Xiao RP2350 over UART
        (RP2350 then forwards the report to the PC as a USB HID device)

 Threading model:
   The ESP-IDF USB host driver runs USB events on its own task (core 0).
   The HID host task delivers report callbacks on core 0.
   The Arduino loop() runs on core 1.

   We forward reports straight to the UART link from the callback —
   HardwareSerial::write is task-safe with internal locking. The hotkey
   callback that triggers a KVM switch only sets a volatile flag in
   main.cpp, which is the same pattern the previous version used.
*********************************************************************/

#include "usb_hid_host.h"
#include "hid_link.h"
#include "hotkey_config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "usb/usb_host.h"
#include "hid_host.h"
#include "hid_usage_keyboard.h"
#include "driver/periph_ctrl.h"
#include "driver/gpio.h"
#include "soc/periph_defs.h"

// ── Module state ──────────────────────────────────────────────────────────────
static QueueHandle_t hid_event_queue   = nullptr;
static volatile bool keyboard_present  = false;
static volatile uint32_t last_report_ms = 0;   // 0 = never received a report
static bool          hotkey_active     = false;
static void (*switch_callback)()       = nullptr;

typedef struct {
    hid_host_device_handle_t hid_device_handle;
    hid_host_driver_event_t  event;
    void*                    arg;
} hid_event_t;

// ── Hotkey filter ─────────────────────────────────────────────────────────────
// Returns true if the report should be forwarded down the link to the PC.
static bool processReport(const uint8_t* report) {
    uint8_t mod    = report[0];
    uint8_t hk_key = getHotkeyKey();
    uint8_t hk_mod = getHotkeyMod();

    bool hotkey_key_down = false;
    for (int i = 2; i < 8; i++) {
        if (report[i] == hk_key) { hotkey_key_down = true; break; }
    }

    bool mod_match = (hk_mod == 0x00) || ((mod & hk_mod) == hk_mod);

    if (!hotkey_active && mod_match && hotkey_key_down) {
        hotkey_active = true;
        Log.println("[HID] Hotkey -> KVM switch");
        // Release any forwarded modifiers/keys so the PC doesn't see a stuck key
        static const uint8_t empty[8] = {0};
        hidLinkSendKeyboard(empty);
        if (switch_callback) switch_callback();
        return false;
    }

    if (hotkey_active) {
        bool all_up = (mod == 0);
        for (int i = 2; i < 8; i++) if (report[i] != 0) { all_up = false; break; }
        if (all_up) hotkey_active = false;
        return false;
    }

    return true;
}

// ── HID report callback (boot-protocol keyboard) ─────────────────────────────
static void hid_keyboard_report_callback(const uint8_t* data, int length) {
    if (length < 8) return;
    last_report_ms = millis();
    if (last_report_ms == 0) last_report_ms = 1;  // 0 reserved for "never"
    if (processReport(data)) hidLinkSendKeyboard(data);
}

// ── HID interface event callback ─────────────────────────────────────────────
static void hid_host_interface_callback(hid_host_device_handle_t handle,
                                        const hid_host_interface_event_t event,
                                        void* arg) {
    uint8_t data[64] = {0};
    size_t  data_length = 0;
    hid_host_dev_params_t dev_params;
    if (hid_host_device_get_params(handle, &dev_params) != ESP_OK) return;

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        if (hid_host_device_get_raw_input_report_data(
                handle, data, sizeof(data), &data_length) == ESP_OK) {
            if (dev_params.sub_class == HID_SUBCLASS_BOOT_INTERFACE &&
                dev_params.proto    == HID_PROTOCOL_KEYBOARD) {
                hid_keyboard_report_callback(data, data_length);
            }
            // Mouse + generic reports are silently dropped — keyboard-only KVM.
        }
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        if (dev_params.proto == HID_PROTOCOL_KEYBOARD) keyboard_present = false;
        Log.printf("[HID] disconnected (proto=%d)\n", dev_params.proto);
        hid_host_device_close(handle);
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        Log.printf("[HID] transfer error (proto=%d)\n", dev_params.proto);
        break;
    default:
        break;
    }
}

// ── HID device event (connect / disconnect at the device level) ──────────────
static void hid_host_device_event(hid_host_device_handle_t handle,
                                  const hid_host_driver_event_t event, void* arg) {
    hid_host_dev_params_t dev_params;
    if (hid_host_device_get_params(handle, &dev_params) != ESP_OK) return;

    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        const hid_host_device_config_t cfg = {
            .callback     = hid_host_interface_callback,
            .callback_arg = nullptr,
        };
        if (hid_host_device_open(handle, &cfg) != ESP_OK) return;

        if (dev_params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
            hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_BOOT);
            if (dev_params.proto == HID_PROTOCOL_KEYBOARD) {
                hid_class_request_set_idle(handle, 0, 0);
                keyboard_present = true;
                Log.println("[HID] keyboard connected");
            }
        }
        hid_host_device_start(handle);
    }
}

// Bridges the ESP-IDF HID-host callback (which runs in driver context and
// can't make blocking USB calls) onto a FreeRTOS task that owns the device.
static void hid_host_device_callback_isr(hid_host_device_handle_t handle,
                                         const hid_host_driver_event_t event,
                                         void* arg) {
    hid_event_t evt = { handle, event, arg };
    if (hid_event_queue) xQueueSend(hid_event_queue, &evt, 0);
}

// ── USB host library task (core 0) ───────────────────────────────────────────
static void usb_lib_task(void* arg) {
    const usb_host_config_t host_cfg = {
        .skip_phy_setup = false,
        .intr_flags     = ESP_INTR_FLAG_LEVEL1,
    };
    if (usb_host_install(&host_cfg) != ESP_OK) {
        Log.println("[USB] usb_host_install failed");
        vTaskDelete(nullptr);
        return;
    }
    xTaskNotifyGive((TaskHandle_t)arg);

    while (true) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
    }
}

// ── HID device-event drain task (core 0) ─────────────────────────────────────
static void hid_event_task(void* arg) {
    hid_event_t evt;
    while (true) {
        if (xQueueReceive(hid_event_queue, &evt, portMAX_DELAY)) {
            hid_host_device_event(evt.hid_device_handle, evt.event, evt.arg);
        }
    }
}

// ── Public API ───────────────────────────────────────────────────────────────
void setupUSBHost() {
    // ESP.restart() (and OTA-triggered restart) is a CPU-only soft reset, so
    // the USB-OTG controller keeps whatever state it had before the reboot
    // and the connected keyboard never sees VBUS drop (its 5V comes from the
    // RP2350 rail, not the ESP32's USB-C). Without intervention the keyboard
    // stays in its previous "configured" state and the device stays dead
    // until a physical replug. periph_module_disable + enable asserts reset
    // and gates the module clock — more thorough than periph_module_reset's
    // pulse — to give the controller a fresh start.
    periph_module_disable(PERIPH_USB_MODULE);
    delay(100);
    periph_module_enable(PERIPH_USB_MODULE);
    delay(50);

    Log.println("[USB] starting native USB host");

    // 1. USB library task — handles bus events, must come up first.
    BaseType_t ok = xTaskCreatePinnedToCore(
        usb_lib_task, "usb_lib", 4096,
        xTaskGetCurrentTaskHandle(), 2, nullptr, 0);
    if (ok != pdPASS) {
        Log.println("[USB] failed to start usb_lib task");
        return;
    }
    // Wait up to 1s for usb_host_install() to complete.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

    // 2. HID-host driver — installs the class driver on top of the USB host.
    hid_event_queue = xQueueCreate(10, sizeof(hid_event_t));
    const hid_host_driver_config_t hid_cfg = {
        .create_background_task = true,
        .task_priority          = 5,
        .stack_size             = 4096,
        .core_id                = 0,
        .callback               = hid_host_device_callback_isr,
        .callback_arg           = nullptr,
    };
    if (hid_host_install(&hid_cfg) != ESP_OK) {
        Log.println("[USB] hid_host_install failed");
        return;
    }

    // 3. Drain task — moves driver-context events onto a normal task so we
    //    can call usb_host APIs from them.
    xTaskCreatePinnedToCore(hid_event_task, "hid_evt", 4096, nullptr, 4, nullptr, 0);

    Log.println("[USB] host ready - plug in keyboard");
}

void usbHidLoop() {
    // Native USB host runs in its own FreeRTOS tasks; nothing to pump here.
}

bool usbHidKeyboardConnected() {
    return keyboard_present;
}

uint32_t usbHidMillisSinceLastReport() {
    if (last_report_ms == 0) return UINT32_MAX;
    return millis() - last_report_ms;
}

void usbHidSetSwitchCallback(void (*cb)()) {
    switch_callback = cb;
}

void usbHidShutdown() {
    Log.println("[USB] shutting down host stack");
    // Best-effort teardown. Order matters: HID class driver first (closes
    // device handles), then free all devices in the host lib, then uninstall
    // the lib itself. Errors here are not actionable — we're about to
    // ESP.restart() anyway — so we ignore return codes.
    hid_host_uninstall();
    delay(50);
    usb_host_device_free_all();
    delay(50);
    usb_host_uninstall();
    delay(100);
}

void usbHidForceBusReset() {
    // Detach D+/D- from the USB-OTG controller's IO-mux and drive both LOW
    // as plain GPIOs. The USB spec defines this state (single-ended-zero
    // for >2.5 us, in practice >=10 ms is conventional) as a bus reset —
    // any device on the bus is required to return to its Default state.
    // Doing this before a chip reset gives the keyboard a deterministic
    // re-enumeration trigger that none of the chip-level resets we've tried
    // were producing on their own.
    Log.println("[USB] forcing bus reset (SE0 on D+/D-)");
    gpio_reset_pin(GPIO_NUM_19);   // D+
    gpio_reset_pin(GPIO_NUM_20);   // D-
    pinMode(19, OUTPUT);
    pinMode(20, OUTPUT);
    digitalWrite(19, LOW);
    digitalWrite(20, LOW);
    delay(20);
}
