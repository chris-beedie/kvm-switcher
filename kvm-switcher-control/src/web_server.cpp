#include "web_server.h"
#include "config.h"
#include "ddc_ci.h"
#include "web_ui.h"
#include "led_debug.h"
#include "hotkey_config.h"
#include "hid_link.h"
#include "settings.h"
#include "mqtt_client.h"
#include "wifi_setup.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

// Shared state — defined in main.cpp
extern bool current_input1;
extern bool monitors_awake;
extern bool last_ddc_a_ok;
extern bool last_ddc_b_ok;
extern volatile bool    api_switch_requested;
extern volatile int     api_set_input;
extern volatile uint8_t api_set_hotkey_key;
extern volatile uint8_t api_set_hotkey_mod;
extern volatile bool    debug_led_set;
extern volatile uint8_t debug_led_r, debug_led_g, debug_led_b;

static AsyncWebServer server(80);

// Settings changes from async handlers are deferred to the Arduino task —
// NVS writes and ESP.restart() must not run on the AsyncTCP stack.
static volatile bool mqtt_save_pending = false;
static volatile bool wifi_reset_pending = false;
static String pending_mqtt_host;
static String pending_mqtt_user;
static String pending_mqtt_pass;
static uint16_t pending_mqtt_port = 1883;

// ================================================================
// Status JSON (also used by mqtt_client)
// ================================================================

String getStatusJSON() {
  LinkUsbStatus usb = hidLinkUsbStatus();
  JsonDocument doc;
  doc["input"]        = current_input1 ? 1 : 2;
  doc["monitors_awake"] = monitors_awake;
  doc["ddc_a_ok"]     = last_ddc_a_ok;
  doc["ddc_b_ok"]     = last_ddc_b_ok;
  doc["input_name_a"] = inputNameU38(current_input1);
  doc["input_name_b"] = inputNameU24(current_input1);
  doc["wifi_rssi"]    = WiFi.RSSI();
  doc["uptime"]       = millis() / 1000;
  doc["ip"]           = WiFi.localIP().toString();
  doc["hid_link_up"]  = hidLinkIsUp();
  doc["usb_mounted"]  = usb.mounted;
  doc["usb_suspended"] = usb.suspended;
  String json;
  serializeJson(doc, json);
  return json;
}

// ================================================================
// Web server setup
// ================================================================

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  server.on("/api/status", HTTP_ANY, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", getStatusJSON());
  });

  server.on("/api/switch", HTTP_ANY, [](AsyncWebServerRequest *request) {
    api_switch_requested = true;
    request->send(200, "application/json", "{\"status\":\"switching\"}");
  });

  server.on("/api/input/1", HTTP_ANY, [](AsyncWebServerRequest *request) {
    api_set_input = 1;
    request->send(200, "application/json", "{\"status\":\"switching\"}");
  });

  server.on("/api/input/2", HTTP_ANY, [](AsyncWebServerRequest *request) {
    api_set_input = 2;
    request->send(200, "application/json", "{\"status\":\"switching\"}");
  });

  // GET /api/hotkey/set?key=N&mod=M — queue a hotkey update for loop()
  // Registered before /api/hotkey to avoid prefix-match ambiguity.
  server.on("/api/hotkey/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("key")) {
      request->send(200, "application/json", "{\"ok\":false,\"error\":\"missing key param\"}");
      return;
    }
    int key = request->getParam("key")->value().toInt();
    if (key < 1 || key > 255) {
      request->send(200, "application/json", "{\"ok\":false,\"error\":\"invalid key\"}");
      return;
    }
    api_set_hotkey_key = (uint8_t)key;
    api_set_hotkey_mod = request->hasParam("mod")
                           ? (uint8_t)request->getParam("mod")->value().toInt()
                           : 0;
    Log.printf("[WEB] Hotkey set: key=%d mod=%d\n", key, api_set_hotkey_mod);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // GET /api/hotkey — return current hotkey as JSON
  server.on("/api/hotkey", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"key\":" + String(getHotkeyKey()) +
                  ",\"mod\":" + String(getHotkeyMod()) +
                  ",\"label\":\"" + hotkeyLabel() + "\"}";
    request->send(200, "application/json", json);
  });

  // GET /api/mqtt — return current MQTT settings
  server.on("/api/mqtt", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["host"] = getMqttHost();
    doc["port"] = getMqttPort();
    doc["user"] = getMqttUser();
    doc["pass"] = getMqttPass();
    String json; serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // POST /api/mqtt/set — form-urlencoded host/port/user/pass; queues save
  server.on("/api/mqtt/set", HTTP_POST, [](AsyncWebServerRequest *request) {
    pending_mqtt_host = request->hasParam("host", true)
                          ? request->getParam("host", true)->value() : String("");
    pending_mqtt_user = request->hasParam("user", true)
                          ? request->getParam("user", true)->value() : String("");
    pending_mqtt_pass = request->hasParam("pass", true)
                          ? request->getParam("pass", true)->value() : String("");
    int port = request->hasParam("port", true)
                 ? request->getParam("port", true)->value().toInt() : 1883;
    if (port < 1 || port > 65535) port = 1883;
    pending_mqtt_port = (uint16_t)port;
    mqtt_save_pending = true;
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // POST /api/wifi/reset — wipe WiFi creds + MQTT settings, reboot to portal
  server.on("/api/wifi/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"ok\":true}");
    wifi_reset_pending = true;
  });

  ElegantOTA.begin(&server);

  server.on("/debug/led", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("r") && request->hasParam("g") && request->hasParam("b")) {
      debug_led_r = request->getParam("r")->value().toInt();
      debug_led_g = request->getParam("g")->value().toInt();
      debug_led_b = request->getParam("b")->value().toInt();
      debug_led_set = true;
      String json = "{\"r\":" + String(debug_led_r) +
                    ",\"g\":" + String(debug_led_g) +
                    ",\"b\":" + String(debug_led_b) + "}";
      request->send(200, "application/json", json);
    } else {
      request->send(200, "text/html", LED_DEBUG_HTML);
    }
  });

  server.begin();
  Log.println("Web server started (OTA at /update)");
}

void webServerLoop() {
  ElegantOTA.loop();
  saveHotkeyIfPending();  // flush NVS write from Arduino task — never from async handler

  if (mqtt_save_pending) {
    mqtt_save_pending = false;
    saveMqttSettings(pending_mqtt_host.c_str(), pending_mqtt_port,
                     pending_mqtt_user.c_str(), pending_mqtt_pass.c_str());
    mqttReconfigure();
  }

  if (wifi_reset_pending) {
    wifi_reset_pending = false;
    delay(100);              // let the response flush before WiFi goes down
    wifiResetAndReboot();    // does not return
  }
}
