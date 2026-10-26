#include "forecast.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Mozilla root certificate bundle that ships with the ESP32 core (used to verify api.open-meteo.com)
extern const uint8_t rootca_bundle_start[] asm("_binary_x509_crt_bundle_start");

bool fetchForecast(const Spot& spot, Forecast& out) {
  char url[360];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&hourly=wind_speed_10m,wind_gusts_10m,wind_direction_10m,weather_code,temperature_2m"
           "&wind_speed_unit=kn&timezone=auto&forecast_days=%d&models=best_match&cell_selection=%s",
           spot.lat, spot.lon, FETCH_DAYS, spot.sea ? "sea" : "land");

  WiFiClientSecure client;
  client.setCACertBundle(rootca_bundle_start);
  HTTPClient http;
  http.setTimeout(15000);
  http.setUserAgent(FW_NAME "/" FW_VERSION);
  if (!http.begin(client, url)) return false;

  int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("%s: HTTP error %d\n", spot.name, status);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  // Only keep the fields we use
  JsonDocument filter;
  filter["utc_offset_seconds"] = true;
  filter["hourly"] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("%s: JSON error %s\n", spot.name, err.c_str());
    return false;
  }

  JsonObject hourly = doc["hourly"];
  JsonArray time = hourly["time"];
  JsonArray wind = hourly["wind_speed_10m"];
  JsonArray gust = hourly["wind_gusts_10m"];
  JsonArray dir = hourly["wind_direction_10m"];
  JsonArray code = hourly["weather_code"];
  JsonArray temp = hourly["temperature_2m"];
  const int hours = FETCH_DAYS * 24;
  if (time.size() < hours || wind.size() < hours || gust.size() < hours ||
      dir.size() < hours || code.size() < hours || temp.size() < hours) {
    Serial.printf("%s: incomplete data\n", spot.name);
    return false;
  }

  const char* t0 = time[0] | "";
  if (sscanf(t0, "%d-%d-%d", &out.year, &out.month, &out.day) != 3) return false;
  out.utcOffset = doc["utc_offset_seconds"] | 0;

  for (int d = 0; d < FETCH_DAYS; d++) {
    DayTemp& t = out.temp[d];
    t.min = 99;
    t.max = -99;
    for (int h = 7; h <= 21; h++) {
      if (temp[d * 24 + h].isNull()) continue;
      float v = temp[d * 24 + h];
      t.min = min(t.min, v);
      t.max = max(t.max, v);
    }
    for (int s = 0; s < SLOTS; s++) {
      int i = d * 24 + SLOT_HOURS[s];
      Slot& slot = out.slot[d][s];
      slot.valid = !wind[i].isNull() && !gust[i].isNull() && !dir[i].isNull();
      slot.wind = wind[i] | 0.0f;
      slot.gust = gust[i] | 0.0f;
      slot.dir = dir[i] | 0;
      // Weather over the 3 h window around the hour
      int worst = 0;
      for (int k = max(0, i - 1); k <= min(hours - 1, i + 1); k++) worst = max(worst, code[k] | 0);
      slot.code = worst;
    }
  }
  out.fetched = ::time(nullptr);
  out.valid = true;
  return true;
}
