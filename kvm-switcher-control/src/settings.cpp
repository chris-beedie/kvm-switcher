#include "settings.h"
#include "config.h"
#include "hid_link.h"
#include <Preferences.h>

static const char* NVS_NS = "kvm";

static String   s_mqtt_host;
static uint16_t s_mqtt_port = 1883;
static String   s_mqtt_user;
static String   s_mqtt_pass;

void loadSettings() {
  Preferences prefs;
  prefs.begin(NVS_NS, /*readOnly=*/true);
  s_mqtt_host = prefs.getString("mqtt_host", "");
  s_mqtt_port = prefs.getUShort("mqtt_port", 1883);
  s_mqtt_user = prefs.getString("mqtt_user", "");
  s_mqtt_pass = prefs.getString("mqtt_pass", "");
  prefs.end();
  Log.printf("[CFG] MQTT host=%s:%u user=%s\n",
             s_mqtt_host.length() ? s_mqtt_host.c_str() : "(unset)",
             s_mqtt_port,
             s_mqtt_user.length() ? s_mqtt_user.c_str() : "(none)");
}

void saveMqttSettings(const char* host, uint16_t port,
                      const char* user, const char* pass) {
  s_mqtt_host = host ? host : "";
  s_mqtt_port = port ? port : 1883;
  s_mqtt_user = user ? user : "";
  s_mqtt_pass = pass ? pass : "";

  Preferences prefs;
  prefs.begin(NVS_NS, /*readOnly=*/false);
  prefs.putString("mqtt_host", s_mqtt_host);
  prefs.putUShort("mqtt_port", s_mqtt_port);
  prefs.putString("mqtt_user", s_mqtt_user);
  prefs.putString("mqtt_pass", s_mqtt_pass);
  prefs.end();
  Log.printf("[CFG] MQTT saved: host=%s:%u user=%s\n",
             s_mqtt_host.c_str(), s_mqtt_port,
             s_mqtt_user.length() ? s_mqtt_user.c_str() : "(none)");
}

void clearMqttSettings() {
  Preferences prefs;
  prefs.begin(NVS_NS, /*readOnly=*/false);
  prefs.remove("mqtt_host");
  prefs.remove("mqtt_port");
  prefs.remove("mqtt_user");
  prefs.remove("mqtt_pass");
  prefs.end();
  s_mqtt_host = "";
  s_mqtt_port = 1883;
  s_mqtt_user = "";
  s_mqtt_pass = "";
}

const char* getMqttHost() { return s_mqtt_host.c_str(); }
uint16_t    getMqttPort() { return s_mqtt_port; }
const char* getMqttUser() { return s_mqtt_user.c_str(); }
const char* getMqttPass() { return s_mqtt_pass.c_str(); }
