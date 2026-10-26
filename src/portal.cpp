#include "portal.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "power.h"
#include "render.h"
#include "web_page.h"

static WebServer server(80);
static DNSServer dns;
static Settings* cfg;
static bool apMode;
static uint32_t lastActivity;
static bool saved;

// WiFi connection attempt started from the setup page
enum JoinState { JOIN_IDLE, JOIN_CONNECTING, JOIN_OK, JOIN_FAIL };
static JoinState joinState;
static uint32_t joinStart, joinOkAt;
static char joinSsid[33], joinPass[65];
static Lang joinLang;

static void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

static void sendError(const char* msg) {
  JsonDocument doc;
  doc["error"] = msg;
  String out;
  serializeJson(doc, out);
  sendJson(400, out);
}

static void handlePage() {
  lastActivity = millis();
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html; charset=utf-8", (const char*)WEB_PAGE_GZ, WEB_PAGE_GZ_LEN);
}

static void handleState() {
  lastActivity = millis();
  if (apMode) {
    JsonDocument doc;
    doc["mode"] = "ap";
    doc["version"] = FW_VERSION;
    String out;
    serializeJson(doc, out);
    sendJson(200, out);
    return;
  }
  String out = settingsToJson(*cfg);
  out.remove(out.length() - 1);  // append the mode to the settings object
  out += ",\"mode\":\"sta\"}";
  sendJson(200, out);
}

static void handleScan() {
  lastActivity = millis();
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    n = WIFI_SCAN_RUNNING;
  }
  JsonDocument doc;
  if (n == WIFI_SCAN_RUNNING) {
    doc["scanning"] = true;
  } else {
    JsonArray list = doc["networks"].to<JsonArray>();
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i).isEmpty()) continue;
      JsonObject o = list.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    if (server.hasArg("refresh")) {
      WiFi.scanDelete();
      WiFi.scanNetworks(true);
    }
  }
  String out;
  serializeJson(doc, out);
  sendJson(200, out);
}

static void handleJoin() {
  lastActivity = millis();
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) return sendError("invalid JSON");
  const char* ssid = doc["ssid"] | "";
  const char* pass = doc["pass"] | "";
  if (!ssid[0] || strlen(ssid) >= sizeof(joinSsid) || strlen(pass) >= sizeof(joinPass))
    return sendError("invalid WiFi");
  strlcpy(joinSsid, ssid, sizeof(joinSsid));
  strlcpy(joinPass, pass, sizeof(joinPass));
  joinLang = langFromCode(doc["lang"] | "en");
  WiFi.disconnect(false);
  WiFi.begin(joinSsid, joinPass);
  joinState = JOIN_CONNECTING;
  joinStart = millis();
  sendJson(200, "{\"ok\":true}");
}

static void handleJoinState() {
  lastActivity = millis();
  static const char* NAMES[] = {"idle", "connecting", "ok", "fail"};
  JsonDocument doc;
  doc["state"] = NAMES[joinState];
  if (joinState == JOIN_OK) doc["ip"] = WiFi.localIP().toString();
  String out;
  serializeJson(doc, out);
  sendJson(200, out);
}

static void handleSettings() {
  lastActivity = millis();
  const char* err = settingsFromJson(server.arg("plain").c_str(), *cfg);
  if (err) return sendError(err);
  settingsSave(*cfg);
  saved = true;
  sendJson(200, "{\"ok\":true}");
}

// Phones probe these URLs to detect a captive portal; redirecting them opens the setup page.
static void handleNotFound() {
  if (apMode) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

static void startServer() {
  static bool routes = false;
  if (!routes) {
    server.on("/", HTTP_GET, handlePage);
    server.on("/api/state", HTTP_GET, handleState);
    server.on("/api/scan", HTTP_GET, handleScan);
    server.on("/api/wifi", HTTP_POST, handleJoin);
    server.on("/api/wifi", HTTP_GET, handleJoinState);
    server.on("/api/settings", HTTP_POST, handleSettings);
    server.onNotFound(handleNotFound);
    routes = true;
  }
  server.begin();
  lastActivity = millis();
}

static bool middleKeyPressed() { return digitalRead(KEY_REFRESH) == LOW; }

bool runWifiPortal(Settings& s, uint32_t timeoutS, const char* reason) {
  cfg = &s;
  apMode = true;
  joinState = JOIN_IDLE;
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  const IPAddress ip(SETUP_AP_IP);
  WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  WiFi.softAP(SETUP_AP_SSID);
  delay(100);
  dns.start(53, "*", WiFi.softAPIP());
  WiFi.scanNetworks(true);
  startServer();
  Serial.printf("Setup WiFi \"%s\" open, http://%s/\n", SETUP_AP_SSID, WiFi.softAPIP().toString().c_str());
  renderSetupWifi(s, reason);
  waitForButtonsReleased();

  bool connected = false;
  while (true) {
    dns.processNextRequest();
    server.handleClient();
    if (joinState == JOIN_CONNECTING) {
      wl_status_t st = WiFi.status();
      if (st == WL_CONNECTED) {
        joinState = JOIN_OK;
        joinOkAt = millis();
        strlcpy(s.ssid, joinSsid, sizeof(s.ssid));
        strlcpy(s.pass, joinPass, sizeof(s.pass));
        s.lang = joinLang;
        settingsSave(s);
        Serial.printf("Joined \"%s\", IP %s\n", s.ssid, WiFi.localIP().toString().c_str());
      } else if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL || millis() - joinStart > 20000) {
        joinState = JOIN_FAIL;
        WiFi.disconnect(false);
      }
    }
    // Give the phone a few seconds to fetch the result before the setup WiFi goes away
    if (joinState == JOIN_OK && millis() - joinOkAt > 8000) {
      connected = true;
      break;
    }
    if (millis() - lastActivity > timeoutS * 1000UL) break;
    if (middleKeyPressed()) break;
    delay(2);
  }
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(connected ? WIFI_STA : WIFI_OFF);
  return connected;
}

PortalResult runSettingsPortal(Settings& s, uint32_t timeoutS) {
  cfg = &s;
  apMode = false;
  saved = false;
  MDNS.begin(MDNS_NAME);
  MDNS.addService("http", "tcp", 80);
  startServer();
  const String ip = WiFi.localIP().toString();
  Serial.printf("Settings page: http://%s/ (http://%s.local/)\n", ip.c_str(), MDNS_NAME);
  renderSettingsInfo(s, ip.c_str(), s.spotCount == 0);
  waitForButtonsReleased();

  PortalResult result = PORTAL_TIMEOUT;
  while (true) {
    server.handleClient();
    if (saved) {
      // Let the response go out before WiFi is switched off
      uint32_t t = millis();
      while (millis() - t < 500) {
        server.handleClient();
        delay(2);
      }
      result = PORTAL_SAVED;
      break;
    }
    if (millis() - lastActivity > timeoutS * 1000UL) break;
    if (middleKeyPressed()) {
      result = PORTAL_EXIT;
      break;
    }
    delay(2);
  }
  server.stop();
  MDNS.end();
  return result;
}
