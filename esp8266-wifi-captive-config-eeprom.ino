/*
 * ESP8266 Captive-Portal Config Saved to Flash
 * --------------------------------------------
 * Provision WiFi and MQTT settings through a captive portal and persist them
 * as JSON in LittleFS. On boot the node loads the saved config and tries to
 * join WiFi; if there is no config, or the join fails, it falls back to a
 * SoftAP "Fluidwire-Setup" with a DNS catch-all so phones pop up the config
 * page automatically. Hold the FLASH button (GPIO0) for 3 seconds at boot
 * to wipe the saved config.
 *
 * Required libraries:
 *   - ArduinoJson (Benoit Blanchon) - install from Library Manager
 *   - ESP8266 core built-ins: ESP8266WiFi, ESP8266WebServer, DNSServer, LittleFS
 *
 * Board: ESP8266 (NodeMCU / Wemos D1 mini), ESP8266 core installed via Boards Mgr.
 * Wiring: none. Uses the on-board FLASH button (GPIO0) and LED.
 *
 * No config.h needed - credentials are entered in the portal at runtime.
 *
 * (c) 2026 Fluidwire. Released under the MIT License.
 * https://github.com/fluidwire
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define RESET_PIN       0                 // FLASH button, active LOW
#define RESET_HOLD_MS   3000
#define CONFIG_PATH     "/config.json"
#define AP_SSID         "Fluidwire-Setup"
#define AP_PASSWORD     "fluidwire123"    // change before deploying (>= 8 chars)
#define JOIN_TIMEOUT_MS 15000

struct Config {
  String ssid;
  String pass;
  String mqttHost;
  uint16_t mqttPort = 1883;
  String topic;
};

Config cfg;
ESP8266WebServer server(80);
DNSServer dns;
bool portalMode = false;

bool loadConfig() {
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) return false;
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("Config parse error: %s\n", err.c_str());
    return false;
  }
  cfg.ssid     = doc["ssid"] | "";
  cfg.pass     = doc["pass"] | "";
  cfg.mqttHost = doc["mqttHost"] | "";
  cfg.mqttPort = doc["mqttPort"] | 1883;
  cfg.topic    = doc["topic"] | "fluidwire/esp8266/data";
  return cfg.ssid.length() > 0;
}

bool saveConfig() {
  StaticJsonDocument<512> doc;
  doc["ssid"] = cfg.ssid;
  doc["pass"] = cfg.pass;
  doc["mqttHost"] = cfg.mqttHost;
  doc["mqttPort"] = cfg.mqttPort;
  doc["topic"] = cfg.topic;
  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

void wipeConfig() {
  LittleFS.remove(CONFIG_PATH);
  Serial.println("Config wiped.");
}

String htmlEscape(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

void handleRoot() {
  String h = F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
               "<title>Fluidwire Setup</title></head><body style='font-family:sans-serif;max-width:420px;margin:2em auto'>"
               "<h2>Fluidwire device setup</h2><form method='POST' action='/save'>"
               "WiFi SSID<br><input name='ssid' value='");
  h += htmlEscape(cfg.ssid);
  h += F("' style='width:100%'><br>WiFi password<br><input name='pass' type='password' style='width:100%'><br>"
         "MQTT host<br><input name='host' value='");
  h += htmlEscape(cfg.mqttHost);
  h += F("' style='width:100%'><br>MQTT port<br><input name='port' type='number' value='");
  h += String(cfg.mqttPort);
  h += F("' style='width:100%'><br>MQTT topic<br><input name='topic' value='");
  h += htmlEscape(cfg.topic);
  h += F("' style='width:100%'><br><br><button type='submit'>Save &amp; reboot</button></form></body></html>");
  server.send(200, "text/html", h);
}

void handleSave() {
  cfg.ssid     = server.arg("ssid");
  if (server.arg("pass").length() > 0) cfg.pass = server.arg("pass");
  cfg.mqttHost = server.arg("host");
  cfg.mqttPort = server.arg("port").toInt() > 0 ? server.arg("port").toInt() : 1883;
  cfg.topic    = server.arg("topic");
  if (cfg.ssid.length() == 0) {
    server.send(400, "text/plain", "SSID is required");
    return;
  }
  if (saveConfig()) {
    server.send(200, "text/html", F("<html><body><h3>Saved. Rebooting...</h3></body></html>"));
    Serial.println("Config saved, rebooting.");
    delay(1000);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "Failed to write config");
  }
}

// Captive-portal catch-all: redirect any unknown URL to the config page.
void handleNotFound() {
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

void startPortal() {
  portalMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.printf("Portal mode: join \"%s\" and open http://%s/\n", AP_SSID, ip.toString().c_str());
}

bool joinWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());
  Serial.printf("Joining \"%s\"", cfg.ssid.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < JOIN_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

bool resetHeld() {
  if (digitalRead(RESET_PIN) != LOW) return false;
  unsigned long t0 = millis();
  while (digitalRead(RESET_PIN) == LOW) {
    if (millis() - t0 >= RESET_HOLD_MS) return true;
    delay(20);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  pinMode(RESET_PIN, INPUT_PULLUP);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed, formatting...");
    LittleFS.format();
    LittleFS.begin();
  }

  if (resetHeld()) wipeConfig();

  if (loadConfig()) {
    Serial.println("Config loaded from flash.");
    if (joinWiFi()) {
      Serial.printf("Connected. IP %s, MQTT %s:%u, topic %s\n",
                    WiFi.localIP().toString().c_str(), cfg.mqttHost.c_str(),
                    cfg.mqttPort, cfg.topic.c_str());
      return;
    }
    Serial.println("WiFi join failed - falling back to portal.");
  } else {
    Serial.println("No saved config - starting portal.");
  }
  startPortal();
}

void loop() {
  if (portalMode) {
    dns.processNextRequest();
    server.handleClient();
    return;
  }
  // Normal operation goes here: use cfg.mqttHost / cfg.mqttPort / cfg.topic
  // with your MQTT client of choice (see fluidwire/esp32-mqtt-publisher).
  if (resetHeld()) {
    wipeConfig();
    ESP.restart();
  }
  delay(50);
}
