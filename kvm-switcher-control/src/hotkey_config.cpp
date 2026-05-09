#include "hotkey_config.h"
#include "config.h"
#include "hid_link.h"
#include <Preferences.h>

static uint8_t hk_key = KVM_HOTKEY_KEY;
static uint8_t hk_mod = KVM_HOTKEY_MOD;
static volatile bool pending_save = false;

static const char* NVS_NS = "kvm";

void loadHotkeyConfig() {
  Preferences prefs;
  prefs.begin(NVS_NS, /*readOnly=*/true);
  hk_key = prefs.getUChar("hk_key", KVM_HOTKEY_KEY);
  hk_mod = prefs.getUChar("hk_mod", KVM_HOTKEY_MOD);
  prefs.end();
  Log.printf("[HK] Hotkey loaded: key=0x%02X mod=0x%02X (%s)\n",
             hk_key, hk_mod, hotkeyLabel().c_str());
}

uint8_t getHotkeyKey() { return hk_key; }
uint8_t getHotkeyMod() { return hk_mod; }

void applyHotkeyConfig(uint8_t key, uint8_t mod) {
  hk_key = key;
  hk_mod = mod;
  pending_save = true;
}

void saveHotkeyIfPending() {
  if (!pending_save) return;
  pending_save = false;
  Preferences prefs;
  prefs.begin(NVS_NS, /*readOnly=*/false);
  prefs.putUChar("hk_key", hk_key);
  prefs.putUChar("hk_mod", hk_mod);
  prefs.end();
  Log.printf("[HK] Hotkey saved: key=0x%02X mod=0x%02X (%s)\n",
             hk_key, hk_mod, hotkeyLabel().c_str());
}

String hotkeyLabel() {
  String s = "";
  if (hk_mod & 0x08 || hk_mod & 0x80) s += "Win+";
  if (hk_mod & 0x01 || hk_mod & 0x10) s += "Ctrl+";
  if (hk_mod & 0x02 || hk_mod & 0x20) s += "Shift+";
  if (hk_mod & 0x04 || hk_mod & 0x40) s += "Alt+";

  // A–Z: HID 0x04–0x1D
  if (hk_key >= 0x04 && hk_key <= 0x1D) {
    s += (char)('A' + hk_key - 0x04);
    return s;
  }
  // 1–9: HID 0x1E–0x26, 0: 0x27
  if (hk_key >= 0x1E && hk_key <= 0x26) { s += (char)('1' + hk_key - 0x1E); return s; }
  if (hk_key == 0x27)                    { s += '0'; return s; }
  // F1–F12: HID 0x3A–0x45
  if (hk_key >= 0x3A && hk_key <= 0x45) { s += 'F'; s += String(hk_key - 0x3A + 1); return s; }

  switch (hk_key) {
    case 0x28: s += "Enter";       break;
    case 0x29: s += "Esc";         break;
    case 0x2A: s += "Backspace";   break;
    case 0x2B: s += "Tab";         break;
    case 0x2C: s += "Space";       break;
    case 0x2D: s += "-";           break;
    case 0x2E: s += "=";           break;
    case 0x2F: s += "[";           break;
    case 0x30: s += "]";           break;
    case 0x31: s += "\\";          break;
    case 0x33: s += ";";           break;
    case 0x34: s += "'";           break;
    case 0x35: s += "`";           break;
    case 0x36: s += ",";           break;
    case 0x37: s += ".";           break;
    case 0x38: s += "/";           break;
    case 0x39: s += "Caps Lock";   break;
    case 0x46: s += "Print Screen";break;
    case 0x47: s += "Scroll Lock"; break;
    case 0x48: s += "Pause";       break;
    case 0x49: s += "Insert";      break;
    case 0x4A: s += "Home";        break;
    case 0x4B: s += "Page Up";     break;
    case 0x4C: s += "Delete";      break;
    case 0x4D: s += "End";         break;
    case 0x4E: s += "Page Down";   break;
    case 0x4F: s += "Right";       break;
    case 0x50: s += "Left";        break;
    case 0x51: s += "Down";        break;
    case 0x52: s += "Up";          break;
    case 0x53: s += "Num Lock";    break;
    case 0x64: s += "\\|";         break;
    default: {
      char buf[8];
      snprintf(buf, sizeof(buf), "0x%02X", hk_key);
      s += buf;
    }
  }
  return s;
}
