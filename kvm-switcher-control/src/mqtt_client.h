#pragma once

void setupMQTT();
void mqttLoop();
void mqttPublishState();

// Publish a button event ("short_press" / "long_press") to the HA event
// entity's state topic. Call from button-handler edge transitions.
void mqttPublishLightButtonEvent(const char* event_type);

// Re-read MQTT settings from NVS, disconnect, and re-prepare the client.
// Call after saveMqttSettings() so changes from the Web UI take effect
// without a reboot. Reconnect happens on the next mqttLoop() tick.
void mqttReconfigure();
