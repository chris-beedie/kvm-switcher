#pragma once

void setupMQTT();
void mqttLoop();
void mqttPublishState();

// Re-read MQTT settings from NVS, disconnect, and re-prepare the client.
// Call after saveMqttSettings() so changes from the Web UI take effect
// without a reboot. Reconnect happens on the next mqttLoop() tick.
void mqttReconfigure();
