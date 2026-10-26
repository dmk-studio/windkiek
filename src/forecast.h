#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "settings.h"

struct Slot {
  bool valid;
  float wind;    // kn
  float gust;    // kn
  int16_t dir;   // degrees, direction the wind comes from
  uint8_t code;  // WMO weather code (worst in the 3 h window)
};

struct DayTemp {
  float min, max;  // daytime 07-21 h
};

struct Forecast {
  bool valid;
  int year, month, day;  // date of day 0 (local time at the spot)
  int32_t utcOffset;     // seconds, local time at the spot = UTC + utcOffset
  time_t fetched;        // UTC
  Slot slot[FETCH_DAYS][SLOTS];
  DayTemp temp[FETCH_DAYS];
};

bool fetchForecast(const Spot& spot, Forecast& out);
