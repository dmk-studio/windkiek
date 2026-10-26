#pragma once
#include "settings.h"

// Display texts per language (UTF-8, rendered with the Latin-1 fonts).
struct Texts {
  const char* weekdays[7];  // Sunday first, upper case
  const char* weekdaysShort[7];
  const char* today;
  const char* updated;      // "Updated Mon 12.10. 14:05"
  const char* offline;
  const char* wind;
  const char* rideable;     // legend for black bars: minimum wind and good direction
  const char* gusts;
  const char* direction;
  const char* noData;
  const char* noWifi;       // %s = network name
  const char* apiFailed;
  const char* retryIn;      // %lu = minutes
  const char* holdForSettings;
  const char* settingsTitle;
  const char* settingsScan;
  const char* settingsOpen;
  const char* settingsTimeout;
  const char* noSpots;
  const char* setupClosed;
  const char* pressToRestart;
};

const Texts& texts(Lang lang);

// Wind values in the selected unit (data is kept in knots)
float knToUnit(float kn, Unit unit);
int windValue(float kn, Unit unit);  // rounded, Beaufort as force 0-12
const char* unitLabel(Unit unit);
