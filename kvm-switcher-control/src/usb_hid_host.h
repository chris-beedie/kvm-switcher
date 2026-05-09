#pragma once
#include <stdint.h>

// Initialise the native ESP32-S3 USB-OTG peripheral as a USB HID host.
// Spawns the USB library + HID host tasks on core 0 (Espressif default).
// Call once from setup() AFTER hidLinkBegin() so we can log over the link.
void setupUSBHost();

// Currently a no-op (host runs in its own FreeRTOS task) but kept for the
// existing main.cpp call site and future per-loop housekeeping.
void usbHidLoop();

// Returns true if a boot-protocol keyboard is currently enumerated.
bool usbHidKeyboardConnected();

// Register a callback invoked when the configured KVM hotkey is pressed.
// The callback runs from the HID host task — keep it short and just set
// a volatile flag for the Arduino loop() to consume.
void usbHidSetSwitchCallback(void (*callback)());
