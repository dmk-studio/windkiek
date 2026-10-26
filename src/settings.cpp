#include "settings.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_crc.h>

static const char* NVS_NS = "wind";  // kept from earlier versions so stored settings survive updates
static const char* LANG_CODES[LANG_COUNT] = {"de", "en", "nl"};
static const char* UNIT_CODES[UNIT_COUNT] = {"kn", "ms", "kmh", "bft"};
static const uint16_t INTERVALS[] = {60, 120, 180, 240};

const char* langCode(Lang l) { return LANG_CODES[l < LANG_COUNT ? l : LANG_EN]; }

Lang langFromCode(const char* code) {
  for (int i = 0; i < LANG_COUNT; i++)
    if (code && strncmp(code, LANG_CODES[i], 2) == 0) return (Lang)i;
  return LANG_EN;
}

static Unit unitFromCode(const char* code) {
  for (int i = 0; i < UNIT_COUNT; i++)
    if (code && strcmp(code, UNIT_CODES[i]) == 0) return (Unit)i;
  return UNIT_KN;
}

void settingsDefaults(Settings& s) {
  memset(&s, 0, sizeof(s));
  s.lang = LANG_EN;
  s.unit = UNIT_KN;
  s.intervalMin = 120;
}

bool settingsHaveWifi(const Settings& s) { return s.ssid[0] != 0; }

// Spots and display options without WiFi, as stored in NVS under "cfg"
static void toJson(const Settings& s, JsonDocument& doc) {
  doc["lang"] = langCode(s.lang);
  doc["unit"] = UNIT_CODES[s.unit];
  doc["interval"] = s.intervalMin;
  JsonArray spots = doc["spots"].to<JsonArray>();
  for (int i = 0; i < s.spotCount; i++) {
    const Spot& p = s.spots[i];
    JsonObject o = spots.add<JsonObject>();
    o["name"] = p.name;
    o["lat"] = serialized(String(p.lat, 4));
    o["lon"] = serialized(String(p.lon, 4));
    o["dirs"] = p.dirs;
    o["min"] = serialized(String(p.minKn, 1));
    o["sea"] = p.sea;
  }
}

// Validates into a copy so a bad request never leaves half-applied settings behind.
static const char* fromJson(JsonDocument& doc, Settings& s) {
  Settings n = s;
  if (doc["lang"].is<const char*>()) n.lang = langFromCode(doc["lang"]);
  if (doc["unit"].is<const char*>()) n.unit = unitFromCode(doc["unit"]);
  if (doc["interval"].is<int>()) {
    int v = doc["interval"];
    bool ok = false;
    for (uint16_t i : INTERVALS) ok |= v == i;
    if (!ok) return "invalid interval";
    n.intervalMin = v;
  }
  if (doc["spots"].is<JsonArray>()) {
    JsonArray spots = doc["spots"];
    if (spots.size() > MAX_SPOTS) return "too many spots";
    n.spotCount = 0;
    for (JsonObject o : spots) {
      Spot& p = n.spots[n.spotCount];
      const char* name = o["name"] | "";
      if (!name[0]) return "spot name missing";
      strlcpy(p.name, name, sizeof(p.name));
      p.lat = o["lat"] | 999.0f;
      p.lon = o["lon"] | 999.0f;
      if (p.lat < -90 || p.lat > 90 || p.lon < -180 || p.lon > 180) return "invalid coordinates";
      p.dirs = o["dirs"] | 0xFFFF;
      p.minKn = o["min"] | 14.0f;
      if (p.minKn < 0 || p.minKn > 100) return "invalid minimum wind";
      p.sea = o["sea"] | false;
      n.spotCount++;
    }
  }
  if (doc["ssid"].is<const char*>()) {
    const char* ssid = doc["ssid"];
    if (strlen(ssid) == 0 || strlen(ssid) >= sizeof(n.ssid)) return "invalid WiFi name";
    // A new network needs its password; the same network keeps the stored one if none is given.
    const char* pass = doc["pass"] | "";
    if (strcmp(ssid, n.ssid) != 0 || pass[0]) {
      if (strlen(pass) >= sizeof(n.pass)) return "WiFi password too long";
      strlcpy(n.pass, pass, sizeof(n.pass));
    }
    strlcpy(n.ssid, ssid, sizeof(n.ssid));
  }
  s = n;
  return nullptr;
}

void settingsLoad(Settings& s) {
  settingsDefaults(s);
  Preferences prefs;
  if (!prefs.begin(NVS_NS, true)) return;  // nothing stored yet
  prefs.getString("ssid", s.ssid, sizeof(s.ssid));
  prefs.getString("pass", s.pass, sizeof(s.pass));
  String cfg = prefs.getString("cfg", "");
  prefs.end();
  if (cfg.isEmpty()) return;
  JsonDocument doc;
  if (deserializeJson(doc, cfg) == DeserializationError::Ok) fromJson(doc, s);
}

void settingsSave(const Settings& s) {
  JsonDocument doc;
  toJson(s, doc);
  String cfg;
  serializeJson(doc, cfg);
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.putString("ssid", s.ssid);
  prefs.putString("pass", s.pass);
  prefs.putString("cfg", cfg);
  prefs.end();
}

void settingsFactoryReset() {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.clear();
  prefs.end();
}

uint32_t settingsSpotsHash(const Settings& s) {
  uint32_t h = esp_crc32_le(0, (const uint8_t*)&s.spotCount, sizeof(s.spotCount));
  for (int i = 0; i < s.spotCount; i++) {
    const Spot& p = s.spots[i];
    h = esp_crc32_le(h, (const uint8_t*)&p.lat, sizeof(p.lat));
    h = esp_crc32_le(h, (const uint8_t*)&p.lon, sizeof(p.lon));
    h = esp_crc32_le(h, (const uint8_t*)&p.sea, sizeof(p.sea));
  }
  return h;
}

String settingsToJson(const Settings& s) {
  JsonDocument doc;
  toJson(s, doc);
  doc["ssid"] = s.ssid;
  doc["version"] = FW_VERSION;
  doc["maxSpots"] = MAX_SPOTS;
  String out;
  serializeJson(doc, out);
  return out;
}

const char* settingsFromJson(const char* json, Settings& s) {
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return "invalid JSON";
  return fromJson(doc, s);
}

int loadPage() {
  Preferences prefs;
  if (!prefs.begin(NVS_NS, true)) return 0;
  int p = prefs.getInt("page", 0);
  prefs.end();
  return p;
}

void savePage(int page) {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.putInt("page", page);
  prefs.end();
}
