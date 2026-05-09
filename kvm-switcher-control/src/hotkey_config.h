#pragma once
#include <Arduino.h>

// Load hotkey from NVS, falling back to compile-time defaults in config.h.
// Call once from setup() before setupUSBHost().
void loadHotkeyConfig();

uint8_t getHotkeyKey();   // USB HID keycode
uint8_t getHotkeyMod();   // modifier bitmask (HID boot protocol)

// Apply a new hotkey to RAM immediately — safe to call from any task/context.
// The change is active at once; NVS write is deferred until saveHotkeyIfPending().
void applyHotkeyConfig(uint8_t key, uint8_t mod);

// Flush any pending hotkey to NVS — call only from loop() (Arduino task).
void saveHotkeyIfPending();

// Human-readable label, e.g. "Scroll Lock", "Win+Z".
String hotkeyLabel();
