/*********************************************************************
 KVM Switcher controller — Seeed Studio Xiao ESP32-S3
 DDC/CI monitor switching · Web UI · REST API · MQTT (HA auto-discovery)
 USB host (native OTG) · UART link to Xiao RP2350 (kvm-switcher-hid)

 Pin map (Xiao ESP32-S3, 14-pin header)
 ─────────────────────────────────────────────────────────────────────
 D0  GPIO1   I2C SDA_A (Wire)  -> Level shifter -> Monitor A
 D1  GPIO2   I2C SCL_A
 D2  GPIO3   I2C SDA_B (Wire1) -> Level shifter -> Monitor B
 D3  GPIO4   I2C SCL_B
 D4  GPIO5   KVM input button (to GND, INPUT_PULLUP)
 D5  GPIO6   NeoPixel data
 D6  GPIO43  UART1 TX -> RP2350 RX (D7 / GP1)
 D7  GPIO44  UART1 RX <- RP2350 TX (D6 / GP0)
 BOOT GPIO0  Debug button (built-in)
 USB-C GPIO19/20  USB-OTG HOST — keyboard plugs in via USB-A breakout
 5V pin       Powered FROM RP2350 5V/VBUS pin
 ─────────────────────────────────────────────────────────────────────

 Serial output: USB-OTG is in HOST mode so USB-CDC is unavailable. The
 Log object below fans out to UART0 (D6/D7 pads — only useful with an
 external UART probe; D6/D7 are also our link pins so don't probe while
 the link is up) AND to the UART link, where the RP2350 forwards the
 lines out its USB-CDC interface.
*********************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <esp_ota_ops.h>

#include "config.h"
#include "ddc_ci.h"
#include "web_server.h"
#include "mqtt_client.h"
#include "usb_hid_host.h"
#include "hotkey_config.h"
#include "hid_link.h"
#include "wifi_setup.h"
#include "settings.h"

// === Pins ===
#define SDA_A          1   // I2C bus A — Monitor A
#define SCL_A          2
#define SDA_B          3   // I2C bus B — Monitor B
#define SCL_B          4
#define BTN_PIN        5   // Pushbutton (to GND) — KVM input switch
#define NEOPIXEL_PIN   6   // NeoPixel data
#define LINK_TX_PIN   43   // D6 — UART1 TX to RP2350
#define LINK_RX_PIN   44   // D7 — UART1 RX from RP2350
#define DEBUG_BTN_PIN  0   // BOOT button — sends 'b' for HID pipeline test

#define LED_COUNT      1

// === Timing ===
#define BTN_DEBOUNCE_MS        200
#define BTN_LONG_PRESS_MS      3000    // hold ≥ this and release → reboot
#define BTN_FACTORY_RESET_MS   10000   // hold ≥ this and release → wipe creds
#define WAKE_PENDING_MS        2000    // window after wake-tap to retap-switch
#define MONITOR_POLL_AWAKE_MS  5000
#define MONITOR_POLL_ASLEEP_MS 30000
#define MONITOR_SLEEP_DELAY_MS 10000

// HID System Control usage codes (Generic Desktop page).
#define HID_SYS_WAKE_UP 0x83
#define HID_SYS_RELEASE 0x00

// === LED ===
Adafruit_NeoPixel strip(LED_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#define COL_TEAL   strip.Color(COL_TEAL_R,   COL_TEAL_G,   COL_TEAL_B)
#define COL_PURPLE strip.Color(COL_PURPLE_R, COL_PURPLE_G, COL_PURPLE_B)
#define COL_AMBER  strip.Color(COL_AMBER_R,  COL_AMBER_G,  COL_AMBER_B)
#define COL_RED    strip.Color(COL_RED_R,    COL_RED_G,    COL_RED_B)
#define COL_OFF    strip.Color(0, 0, 0)

// === Shared state (extern'd by web_server.cpp and mqtt_client.cpp) ===
bool current_input1 = true;
bool monitors_awake = true;
bool last_ddc_a_ok  = true;
bool last_ddc_b_ok  = true;

// API flags — set by web/MQTT handlers, consumed by loop()
volatile bool    api_switch_requested = false;
volatile int     api_set_input        = 0;
volatile bool    api_wake_requested   = false;
volatile uint8_t api_set_hotkey_key   = 0;   // non-zero = pending hotkey change
volatile uint8_t api_set_hotkey_mod   = 0;

// Debug LED — set by web handler, applied by loop()
volatile bool    debug_led_set = false;
volatile uint8_t debug_led_r = 0, debug_led_g = 0, debug_led_b = 0;

// === Button state ===
bool          btn_pressed     = false;
unsigned long btn_press_start = 0;
unsigned long last_btn_action = 0;
int           btn_threshold   = 0;     // 0 = none, 1 = >=3s, 2 = >=10s (LED feedback only)
bool          dbg_btn_pressed = false;

// === Wake-pending state ===
// While monitors are asleep, the first tap "arms" wake mode for WAKE_PENDING_MS
// instead of doing a normal switch. A second tap during that window toggles
// the intended input (no DDC). On expiry we fire the wake sequence to whichever
// input is now selected.
bool          wake_pending           = false;
unsigned long wake_pending_started   = 0;
bool          wake_input_at_arm      = false;

// === Monitor sleep state ===
unsigned long monitor_off_since = 0;
unsigned long last_monitor_poll = 0;

// === Link state (one-shot logging when link transitions) ===
bool link_was_up = false;

// === Forward declarations ===
void setLED(uint32_t colour);
void flashLED(uint32_t colour, int times, int ms);
void doSwitch(const char* source);
void handleButton();
void handleDebugButton();
void checkMonitorSleep();
void handleWakePending();
void fireWakeSequence();
uint32_t idleLEDColour();

uint32_t inputColour() { return current_input1 ? COL_TEAL : COL_PURPLE; }

// Default LED colour when nothing is actively painting it (no DDC/wake/etc).
// During wake-pending we keep showing input colour so the user can see which
// input the upcoming wake will target.
uint32_t idleLEDColour() {
  if (monitors_awake || wake_pending) return inputColour();
  return COL_OFF;
}

// ================================================================
// Setup
// ================================================================

void setup() {
  esp_ota_mark_app_valid_cancel_rollback();  // confirm this app is good so OTA can proceed

  Serial.begin(115200);                      // hardware UART0 — silent unless probed
  hidLinkBegin(LINK_TX_PIN, LINK_RX_PIN);    // up before any Log.print so logs reach RP2350

  delay(200);
  Log.println("\n=== KVM Switcher (Xiao ESP32-S3) ===");

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(DEBUG_BTN_PIN, INPUT_PULLUP);

  strip.begin();
  strip.setBrightness(30);
  setLED(COL_AMBER);

  Wire.begin(SDA_A, SCL_A);
  Wire.setClock(100000);
  Wire1.begin(SDA_B, SCL_B);
  Wire1.setClock(100000);

  int detected = ddcReadCurrentInput(Wire);
  if (detected > 0) {
    monitors_awake = true;
    current_input1 = (detected == 1);
  } else {
    monitors_awake = false;
    current_input1 = true;
  }

  loadHotkeyConfig();
  loadSettings();
  runWifiSetup();
  setupWebServer();
  setupMQTT();
  setupUSBHost();
  usbHidSetSwitchCallback([]() { api_switch_requested = true; });

  setLED(monitors_awake ? inputColour() : COL_OFF);
  if (!monitors_awake) Log.println("Monitor not responding — waiting for wake");

  Log.printf("Ready. Input %d. Monitors %s.\n",
             current_input1 ? 1 : 2, monitors_awake ? "awake" : "asleep");
  if (WiFi.status() == WL_CONNECTED)
    Log.printf("Web UI: http://%s/\n", WiFi.localIP().toString().c_str());
  Log.println();
}

// ================================================================
// Main loop
// ================================================================

void loop() {
  if (api_switch_requested) {
    api_switch_requested = false;
    doSwitch("api");
  }
  if (api_set_input == 1) {
    api_set_input = 0;
    if (!current_input1) doSwitch("api");
  }
  if (api_set_input == 2) {
    api_set_input = 0;
    if (current_input1) doSwitch("api");
  }
  if (api_set_hotkey_key != 0) {
    applyHotkeyConfig(api_set_hotkey_key, api_set_hotkey_mod);
    api_set_hotkey_key = 0;
    // NVS flush happens in webServerLoop() via saveHotkeyIfPending()
  }
  if (api_wake_requested) {
    api_wake_requested = false;
    Log.printf(">>> wake requested via api -> input %d\n", current_input1 ? 1 : 2);
    fireWakeSequence();
  }

  handleButton();
  handleDebugButton();
  handleWakePending();
  checkMonitorSleep();
  mqttLoop();
  webServerLoop();
  hidLinkLoop();
  usbHidLoop();

  // Surface link transitions: log + flash red on drop, log + restore on recovery.
  bool link_now = hidLinkIsUp();
  if (link_now != link_was_up) {
    link_was_up = link_now;
    if (link_now) {
      Log.println("[LINK] up");
      setLED(idleLEDColour());
    } else {
      Log.println("[LINK] down (no heartbeat from RP2350)");
      flashLED(COL_RED, 2, 80);
    }
    mqttPublishState();
  }

  if (debug_led_set) {
    debug_led_set = false;
    strip.setPixelColor(0, strip.Color(debug_led_r, debug_led_g, debug_led_b));
    strip.show();
  }
}

// ================================================================
// Switching
// ================================================================

void doSwitch(const char* source) {
  current_input1 = !current_input1;
  Log.printf(">>> SWITCH by %s -> Input %d\r\n", source, current_input1 ? 1 : 2);

  setLED(COL_AMBER);
  SwitchResult r = ddcSwitchMonitors(Wire, Wire1, current_input1);
  last_ddc_a_ok = r.a_ok;
  last_ddc_b_ok = r.b_ok;

  int failures = (!r.a_ok ? 1 : 0) + (!r.b_ok ? 1 : 0);
  if (failures > 0) flashLED(COL_RED, failures, 150);

  setLED(inputColour());
  mqttPublishState();
}

// ================================================================
// Buttons
// ================================================================

// Press <50ms: noise. 50ms–3s: switch input. 3s–10s: reboot.
// 10s+: factory reset (wipe WiFi creds + MQTT settings, return to portal).
// Action fires on RELEASE so the user can keep holding past 3s to reach 10s.
// LED previews the threshold while held: red at 3s, purple at 10s.
void handleButton() {
  bool pressed = (digitalRead(BTN_PIN) == LOW);

  if (pressed && !btn_pressed) {
    btn_pressed     = true;
    btn_press_start = millis();
    btn_threshold   = 0;
  } else if (pressed && btn_pressed) {
    unsigned long held = millis() - btn_press_start;
    if (held >= BTN_FACTORY_RESET_MS && btn_threshold != 2) {
      btn_threshold = 2;
      setLED(COL_PURPLE);
    } else if (held >= BTN_LONG_PRESS_MS && btn_threshold == 0) {
      btn_threshold = 1;
      setLED(COL_RED);
    }
  } else if (!pressed && btn_pressed) {
    btn_pressed = false;
    unsigned long held = millis() - btn_press_start;
    if (held >= BTN_FACTORY_RESET_MS) {
      Log.println(">>> 10s LONG PRESS -> factory reset");
      for (int i = 0; i < 5; i++) { setLED(COL_PURPLE); delay(100); setLED(COL_OFF); delay(100); }
      wifiResetAndReboot();
    } else if (held >= BTN_LONG_PRESS_MS) {
      Log.println(">>> 3s LONG PRESS -> reboot");
      for (int i = 0; i < 6; i++) { setLED(COL_RED); delay(80); setLED(COL_OFF); delay(80); }
      ESP.restart();
    } else if (held > 50 && (millis() - last_btn_action > BTN_DEBOUNCE_MS)) {
      last_btn_action = millis();
      if (!monitors_awake) {
        if (!wake_pending) {
          wake_pending         = true;
          wake_pending_started = millis();
          wake_input_at_arm    = current_input1;
          Log.printf(">>> wake armed -> input %d (tap again within %d ms to switch)\n",
                     current_input1 ? 1 : 2, WAKE_PENDING_MS);
        } else {
          current_input1       = !current_input1;
          wake_pending_started = millis();
          Log.printf(">>> wake target switched -> input %d\n",
                     current_input1 ? 1 : 2);
        }
        setLED(inputColour());
      } else {
        doSwitch("button");
      }
    } else {
      // Restore LED in case the threshold preview painted it but the press
      // ended below the action window (e.g. <50 ms noise tap).
      setLED(idleLEDColour());
    }
    btn_threshold = 0;
  }
}

// ================================================================
// Wake-pending: fire the wake sequence WAKE_PENDING_MS after the
// last arming/retap. If the user changed input during the window,
// best-effort DDC switch to align the displays before sending wake.
// ================================================================

void handleWakePending() {
  if (!wake_pending) return;
  if (millis() - wake_pending_started < WAKE_PENDING_MS) return;
  wake_pending = false;

  if (current_input1 != wake_input_at_arm) {
    Log.printf(">>> wake firing on input %d (DDC switch first)\n",
               current_input1 ? 1 : 2);
    SwitchResult r = ddcSwitchMonitors(Wire, Wire1, current_input1);
    last_ddc_a_ok = r.a_ok;
    last_ddc_b_ok = r.b_ok;  // monitors are likely asleep — failures expected
  } else {
    Log.printf(">>> wake firing on input %d\n", current_input1 ? 1 : 2);
  }

  fireWakeSequence();
  setLED(idleLEDColour());
}

// Send Shift tap + System-Control "Wake Up" tap. Shift wakes S3 USB-host
// resume; the System usage covers laptops that listen for the dedicated
// Generic Desktop wake event instead of generic HID activity.
void fireWakeSequence() {
  const uint8_t shift_down[8] = { 0x02, 0, 0, 0, 0, 0, 0, 0 };  // Left Shift
  const uint8_t shift_up[8]   = { 0 };

  hidLinkSendKeyboard(shift_down);
  delay(50);
  hidLinkSendKeyboard(shift_up);
  delay(80);
  hidLinkSendSystem(HID_SYS_WAKE_UP);
  delay(50);
  hidLinkSendSystem(HID_SYS_RELEASE);
}

// Debug BOOT button — sends 'b' down the link to verify the HID pipeline.
void handleDebugButton() {
  bool pressed = (digitalRead(DEBUG_BTN_PIN) == LOW);
  if (pressed && !dbg_btn_pressed) {
    dbg_btn_pressed = true;
    Log.println("[DBG] BOOT button -> sending 'b'");
    const uint8_t down[8] = { 0, 0, 0x05, 0, 0, 0, 0, 0 };  // HID 'b' = 0x05
    hidLinkSendKeyboard(down);
  } else if (!pressed && dbg_btn_pressed) {
    dbg_btn_pressed = false;
    const uint8_t up[8] = {0};
    hidLinkSendKeyboard(up);
  }
}

// ================================================================
// Monitor sleep detection — slower poll when asleep
// ================================================================

void checkMonitorSleep() {
  unsigned long interval = monitors_awake ? MONITOR_POLL_AWAKE_MS : MONITOR_POLL_ASLEEP_MS;
  if (millis() - last_monitor_poll < interval) return;
  last_monitor_poll = millis();

  bool awake = ddcIsMonitorAwake(Wire);

  if (awake && !monitors_awake) {
    Log.println("Monitor woke up");
    monitors_awake    = true;
    monitor_off_since = 0;

    int detected = ddcReadCurrentInput(Wire);
    if      (detected == 1) current_input1 = true;
    else if (detected == 2) current_input1 = false;

    setLED(inputColour());
    mqttPublishState();

  } else if (!awake && monitors_awake) {
    if (monitor_off_since == 0) {
      monitor_off_since = millis();
    } else if (millis() - monitor_off_since >= MONITOR_SLEEP_DELAY_MS) {
      Log.println("Monitor asleep -> LED off");
      monitors_awake    = false;
      monitor_off_since = 0;
      setLED(COL_OFF);
      mqttPublishState();
    }
  } else if (awake) {
    monitor_off_since = 0;
  }
}

// ================================================================
// LED
// ================================================================

void setLED(uint32_t colour) {
  strip.setPixelColor(0, colour);
  strip.show();
}

void flashLED(uint32_t colour, int times, int ms) {
  uint32_t prev = strip.getPixelColor(0);
  for (int i = 0; i < times; i++) {
    setLED(colour); delay(ms);
    setLED(COL_OFF); delay(ms);
  }
  setLED(prev);
}

