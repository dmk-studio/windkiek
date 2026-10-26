#pragma once
#include <Arduino.h>
#include "config.h"

// User settings, stored in NVS (flash) and edited on the setup page (web/setup.html).

enum Lang : uint8_t { LANG_DE, LANG_EN, LANG_NL, LANG_COUNT };
enum Unit : uint8_t { UNIT_KN, UNIT_MS, UNIT_KMH, UNIT_BFT, UNIT_COUNT };

struct Spot {
  char name[33];  // UTF-8
  float lat, lon;
  uint16_t dirs;  // bit i set = rideable when the wind comes from sector i (i * 22.5°, 0 = N)
  float minKn;    // minimum wind for a black bar, always stored in knots
  bool sea;       // use the Open-Meteo grid cell over water (coastal spots)
};

struct Settings {
  char ssid[33];
  char pass[65];
  Lang lang;
  Unit unit;
  uint16_t intervalMin;  // regular update interval
  int spotCount;
  Spot spots[MAX_SPOTS];
};

void settingsDefaults(Settings& s);
void settingsLoad(Settings& s);
void settingsSave(const Settings& s);
void settingsFactoryReset();
bool settingsHaveWifi(const Settings& s);
// Checksum of everything that affects the forecast data (spots), used to drop stale caches.
uint32_t settingsSpotsHash(const Settings& s);

// JSON for the setup page. The WiFi password is never sent out.
String settingsToJson(const Settings& s);
// Applies JSON from the setup page. Returns an error message or nullptr on success.
const char* settingsFromJson(const char* json, Settings& s);

int loadPage();
void savePage(int page);

const char* langCode(Lang l);
Lang langFromCode(const char* code);
