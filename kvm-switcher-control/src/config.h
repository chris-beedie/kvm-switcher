#pragma once

// === Device identity ===
#define DEVICE_NAME   "kvm-switcher"
#define FRIENDLY_NAME "KVM Switcher"

// === KVM hotkey (intercepted before forwarding to Elite-C) ===
// Scroll Lock (0x47) — classic KVM switch key, no modifier needed, no Windows conflicts.
// To use Win+Z instead: set KVM_HOTKEY_KEY 0x1D, KVM_HOTKEY_MOD (0x08|0x80)
#define KVM_HOTKEY_KEY  0x47   // Scroll Lock
#define KVM_HOTKEY_MOD  0x00   // no modifier required (0x00 = any modifier state is fine)

// === LED colours (R, G, B components) ===
// Used by main.cpp (strip.Color) and led_debug.h (preset buttons / slider defaults)
#define COL_TEAL_R   0
#define COL_TEAL_G   60
#define COL_TEAL_B   25
#define COL_PURPLE_R 70
#define COL_PURPLE_G 15
#define COL_PURPLE_B 100
#define COL_AMBER_R  90
#define COL_AMBER_G  45
#define COL_AMBER_B  0
#define COL_RED_R    255
#define COL_RED_G    0
#define COL_RED_B    0
