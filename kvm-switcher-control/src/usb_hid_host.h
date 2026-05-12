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

// Milliseconds since the last keyboard report was received from the USB host.
// Returns UINT32_MAX if no report has ever been received this boot.
uint32_t usbHidMillisSinceLastReport();

// Register a callback invoked when the configured KVM hotkey is pressed.
// The callback runs from the HID host task — keep it short and just set
// a volatile flag for the Arduino loop() to consume.
void usbHidSetSwitchCallback(void (*callback)());

// Graceful teardown of the HID host + USB host stack. Call right before
// ESP.restart() (e.g. after an OTA upload) so the new firmware finds a
// quiescent peripheral and re-enumerates the keyboard cleanly without a
// physical replug.
void usbHidShutdown();

// Drive D+/D- (GPIO19/20) both LOW for ~20 ms — the on-the-wire encoding
// of a USB bus reset (SE0). Used after usbHidShutdown to force whatever
// device is on the bus back to its Default state before we soft-reset
// the chip; otherwise the keyboard tends to stay in its previously
// enumerated state across an ESP.restart.
void usbHidForceBusReset();
