#include "mqtt_client.h"
#include "config.h"
#include "hid_link.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define MQTT_RECONNECT_MS  5000
#define MQTT_PUBLISH_MS    10000

// Shared state — defined in main.cpp
extern bool current_input1;
extern bool monitors_awake;
extern bool last_ddc_a_ok;
extern bool last_ddc_b_ok;
extern volatile bool api_switch_requested;
extern volatile int  api_set_input;

// Defined in web_server.cpp
String getStatusJSON();

// Module-private state
static WiFiClient    wifiClient;
static PubSubClient  mqtt(wifiClient);
static unsigned long last_mqtt_reconnect = 0;
static unsigned long last_mqtt_publish   = 0;
static bool          mqtt_enabled        = false;
static bool          mqtt_ha_configured  = false;

// ================================================================
// Internal helpers
// ================================================================

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  Log.printf("MQTT: %s = %s\n", topic, msg.c_str());

  String cmd_topic = String("kvm/") + DEVICE_NAME + "/set";
  if (String(topic) == cmd_topic) {
    if      (msg == "1")      api_set_input = 1;
    else if (msg == "2")      api_set_input = 2;
    else if (msg == "toggle") api_switch_requested = true;
  }
}

static void mqttPublishHADiscovery() {
  if (mqtt_ha_configured) return;

  String state_topic = String("kvm/") + DEVICE_NAME + "/state";
  String cmd_topic   = String("kvm/") + DEVICE_NAME + "/set";
  String avail_topic = String("kvm/") + DEVICE_NAME + "/status";

  // Input select entity
  {
    String base = String("homeassistant/select/") + DEVICE_NAME;
    JsonDocument doc;
    doc["name"]              = "Input";
    doc["unique_id"]         = String(DEVICE_NAME) + "_input";
    doc["command_topic"]     = cmd_topic;
    doc["state_topic"]       = state_topic;
    doc["value_template"]    = "{{ value_json.input }}";
    doc["options"][0]        = "1";
    doc["options"][1]        = "2";
    doc["availability_topic"]    = avail_topic;
    doc["payload_available"]     = "online";
    doc["payload_not_available"] = "offline";
    JsonObject dev = doc["device"].to<JsonObject>();
    dev["identifiers"][0] = DEVICE_NAME;
    dev["name"]           = FRIENDLY_NAME;
    dev["model"]          = "DDC/CI KVM";
    dev["manufacturer"]   = "DIY";
    String p; serializeJson(doc, p);
    mqtt.publish((base + "_input/config").c_str(), p.c_str(), true);
  }

  // Monitor awake binary sensor
  {
    String base = String("homeassistant/binary_sensor/") + DEVICE_NAME;
    JsonDocument doc;
    doc["name"]           = "Monitors";
    doc["unique_id"]      = String(DEVICE_NAME) + "_monitors";
    doc["state_topic"]    = state_topic;
    doc["value_template"] = "{{ 'ON' if value_json.monitors_awake else 'OFF' }}";
    doc["device_class"]   = "power";
    doc["availability_topic"] = avail_topic;
    JsonObject dev = doc["device"].to<JsonObject>();
    dev["identifiers"][0] = DEVICE_NAME;
    String p; serializeJson(doc, p);
    mqtt.publish((base + "_monitors/config").c_str(), p.c_str(), true);
  }

  mqtt_ha_configured = true;
  Log.println("MQTT: HA discovery published");
}

// ================================================================
// Public API
// ================================================================

void mqttPublishState() {
  if (!mqtt_enabled || !mqtt.connected()) return;
  String topic = String("kvm/") + DEVICE_NAME + "/state";
  mqtt.publish(topic.c_str(), getStatusJSON().c_str(), true);
}

void setupMQTT() {
  if (strlen(MQTT_HOST) == 0) {
    Log.println("MQTT disabled");
    return;
  }
  mqtt_enabled = true;
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(1024);
  Log.printf("MQTT: %s:%d\n", MQTT_HOST, MQTT_PORT);
}

void mqttLoop() {
  if (!mqtt_enabled) return;

  if (!mqtt.connected()) {
    if (millis() - last_mqtt_reconnect < MQTT_RECONNECT_MS) return;
    last_mqtt_reconnect = millis();

    String avail_topic = String("kvm/") + DEVICE_NAME + "/status";
    bool ok;
    if (strlen(MQTT_USER) > 0)
      ok = mqtt.connect(DEVICE_NAME, MQTT_USER, MQTT_PASS,
                        avail_topic.c_str(), 0, true, "offline");
    else
      ok = mqtt.connect(DEVICE_NAME, nullptr, nullptr,
                        avail_topic.c_str(), 0, true, "offline");

    if (ok) {
      Log.println("MQTT connected");
      mqtt.publish(avail_topic.c_str(), "online", true);
      mqtt.subscribe((String("kvm/") + DEVICE_NAME + "/set").c_str());
      mqttPublishHADiscovery();
      mqttPublishState();
    }
    return;
  }

  mqtt.loop();

  if (millis() - last_mqtt_publish > MQTT_PUBLISH_MS) {
    last_mqtt_publish = millis();
    mqttPublishState();
  }
}
