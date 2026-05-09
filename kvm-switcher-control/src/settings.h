#pragma once

#include <Arduino.h>

// NVS-backed runtime settings. Values persist across reboots in the same
// 'kvm' Preferences namespace as the hotkey config. Empty MQTT host = disabled.

void        loadSettings();
void        saveMqttSettings(const char* host, uint16_t port,
                             const char* user, const char* pass);
void        clearMqttSettings();

const char* getMqttHost();
uint16_t    getMqttPort();
const char* getMqttUser();
const char* getMqttPass();
