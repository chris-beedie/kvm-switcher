#pragma once

// Bring WiFi up. If stored creds work, returns once connected. If not,
// starts an open AP "KVM-Switcher-Setup" with a captive portal at
// 192.168.4.1 where the user enters WiFi + (optional) MQTT settings.
// Reboots if the portal times out.
void runWifiSetup();

// Wipe stored WiFi credentials and MQTT settings, then reboot. Used by
// the factory-reset gesture and the Web UI "Forget WiFi" button.
void wifiResetAndReboot();
