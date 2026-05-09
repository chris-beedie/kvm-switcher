#include "wifi_setup.h"
#include "config.h"
#include "settings.h"
#include "hid_link.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <Adafruit_NeoPixel.h>

extern Adafruit_NeoPixel strip;
extern void setLED(uint32_t colour);

#define COL_BLUE   strip.Color(0, 0, 80)
#define COL_GREEN  strip.Color(0, 80, 0)
#define COL_REDX   strip.Color(255, 0, 0)

#define PORTAL_AP_NAME       "KVM-Switcher-Setup"
#define PORTAL_TIMEOUT_SEC   180
#define WIFI_CONNECT_TIMEOUT 20

static bool s_portal_saved_params = false;

void runWifiSetup() {
  s_portal_saved_params = false;

  WiFiManager wm;
  WiFi.setHostname(DEVICE_NAME);

  // Custom MQTT params. WiFiManager keeps pointers to these for the lifetime
  // of the portal session, so they must outlive autoConnect() — they do,
  // because they live on this stack frame which doesn't return until the
  // portal closes (or autoConnect succeeds without showing the portal).
  char port_str[8];
  snprintf(port_str, sizeof(port_str), "%u", getMqttPort());

  WiFiManagerParameter p_host("mqtt_host", "MQTT host (blank = disable)",
                              getMqttHost(), 64);
  WiFiManagerParameter p_port("mqtt_port", "MQTT port", port_str, 6);
  WiFiManagerParameter p_user("mqtt_user", "MQTT user (blank = anonymous)",
                              getMqttUser(), 64);
  WiFiManagerParameter p_pass("mqtt_pass", "MQTT password",
                              getMqttPass(), 64);
  wm.addParameter(&p_host);
  wm.addParameter(&p_port);
  wm.addParameter(&p_user);
  wm.addParameter(&p_pass);

  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_SEC);
  wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT);
  wm.setSaveParamsCallback([]() { s_portal_saved_params = true; });
  wm.setAPCallback([](WiFiManager*) {
    Log.printf("[WIFI] No creds — captive portal '%s' (192.168.4.1)\n",
               PORTAL_AP_NAME);
    setLED(COL_BLUE);
  });

  Log.println("[WIFI] connecting...");
  bool connected = wm.autoConnect(PORTAL_AP_NAME);

  if (s_portal_saved_params) {
    saveMqttSettings(p_host.getValue(),
                     (uint16_t)atoi(p_port.getValue()),
                     p_user.getValue(),
                     p_pass.getValue());
  }

  if (!connected) {
    Log.println("[WIFI] portal timeout — rebooting");
    delay(500);
    ESP.restart();
  }

  Log.printf("[WIFI] connected (%s)\n", WiFi.localIP().toString().c_str());
}

void wifiResetAndReboot() {
  Log.println("[WIFI] factory reset — wiping creds + MQTT settings");
  clearMqttSettings();
  WiFi.disconnect(/*wifioff=*/true, /*eraseap=*/true);
  delay(500);
  ESP.restart();
}
