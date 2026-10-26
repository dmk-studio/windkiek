// Windkiek – wind forecast for kite/windsurf spots on the Seeed reTerminal E1001
// Data: Open-Meteo (https://open-meteo.com), licensed CC BY 4.0
//
// Each wakeup: evaluate keys -> (setup if needed) -> WiFi -> time (NTP) -> forecast for all spots
// -> draw the current page -> deep sleep until the next update or a key press.
//
// Keys: left/right = previous/next page, middle = update now,
//       middle 3 s = settings page, middle 10 s = factory reset.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "config.h"
#include "forecast.h"
#include "portal.h"
#include "power.h"
#include "render.h"
#include "settings.h"
#include "text.h"

// Kept during deep sleep (lost on power loss or reset)
RTC_DATA_ATTR static Forecast cache[MAX_SPOTS];
RTC_DATA_ATTR static uint32_t cacheHash;   // spots the cache belongs to
RTC_DATA_ATTR static time_t nextUpdate;    // UTC
RTC_DATA_ATTR static int failCount;        // failed updates in a row
RTC_DATA_ATTR static int wifiFails;        // failed WiFi connections in a row

static Settings settings;
static int batteryPct;

static bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == settings.ssid) return true;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.ssid, settings.pass);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(100);
  Serial.printf("WiFi \"%s\": %s\n", settings.ssid, WiFi.status() == WL_CONNECTED ? "connected" : "FAILED");
  return WiFi.status() == WL_CONNECTED;
}

static void wifiOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

static bool timeValid(time_t t) { return t > 1700000000; }

static void syncTime() {
  configTime(0, 0, NTP_SERVER);  // the device clock runs in UTC, spot times come from Open-Meteo
  uint32_t start = millis();
  while (!timeValid(time(nullptr)) && millis() - start < 10000) delay(100);
  if (!timeValid(time(nullptr))) Serial.println("NTP failed");
}

[[noreturn]] static void sleepUntilKey(const char* title, const char* line) {
  wifiOff();
  renderMessage(settings, title, line);
  displaySleep();
  waitForButtonsReleased();
  deepSleepSeconds(0);
}

static const char* noWifiText() {
  static char buf[96];
  snprintf(buf, sizeof(buf), texts(settings.lang).noWifi, settings.ssid);
  return buf;
}

// Settings page; WiFi must be connected. Returns when the forecast can be shown.
static void settingsFlow() {
  PortalResult r = runSettingsPortal(settings, PORTAL_TIMEOUT_S);
  Serial.printf("Settings page closed (%s)\n", r == PORTAL_SAVED ? "saved" : r == PORTAL_EXIT ? "exit" : "timeout");
  if (settings.spotCount == 0) {
    const Texts& tx = texts(settings.lang);
    sleepUntilKey(tx.noSpots, tx.holdForSettings);
  }
}

// WiFi setup through the setup WiFi, then the settings page
static void setupFlow(const char* reason, uint32_t timeoutS) {
  if (!runWifiPortal(settings, timeoutS, reason)) {
    if (settings.spotCount > 0 && settingsHaveWifi(settings)) return;  // keep showing the old forecast
    const Texts& tx = texts(settings.lang);
    sleepUntilKey(tx.setupClosed, tx.pressToRestart);
  }
  connectWifi();  // the switch from setup WiFi to the home WiFi alone can drop the connection
  settingsFlow();
}

static void printForecast(const Spot& spot, const Forecast& fc) {
  Serial.printf("--- %s (%04d-%02d-%02d, UTC%+d h) ---\n", spot.name, fc.year, fc.month, fc.day,
                (int)(fc.utcOffset / 3600));
  for (int d = 0; d < FETCH_DAYS; d++) {
    Serial.printf("Day %d  %4.1f-%4.1f C |", d, fc.temp[d].min, fc.temp[d].max);
    for (int s = 0; s < SLOTS; s++) {
      const Slot& v = fc.slot[d][s];
      Serial.printf(" %02dh %2.0f/%2.0f %3d° W%-2d |", SLOT_HOURS[s], v.wind, v.gust, v.dir, v.code);
    }
    Serial.println();
  }
}

// Fetches all spots, so paging later needs no WiFi. Returns true if every spot was updated.
static bool updateAll() {
  bool wifiOk = connectWifi();
  if (!wifiOk) {
    wifiFails++;
    // Router replaced or password changed: offer the setup WiFi once for a few minutes
    if (wifiFails == WIFI_FAILS_FOR_PORTAL && runWifiPortal(settings, PORTAL_AUTO_TIMEOUT_S, noWifiText()))
      wifiOk = true;
  }
  if (!wifiOk) return false;
  wifiFails = 0;
  syncTime();
  bool ok = true;
  for (int i = 0; i < settings.spotCount; i++) {
    static Forecast fresh;
    fresh = {};
    if (fetchForecast(settings.spots[i], fresh)) {
      cache[i] = fresh;
      printForecast(settings.spots[i], fresh);
    } else {
      ok = false;
    }
  }
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(KEY_NEXT, INPUT);  // pull-ups are on the board
  pinMode(KEY_REFRESH, INPUT);
  pinMode(KEY_PREV, INPUT);
  led(false);
  setenv("TZ", "UTC0", 1);
  tzset();
  Serial.printf("\n%s %s\n", FW_NAME, FW_VERSION);

  settingsLoad(settings);
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const uint64_t wakePins = cause == ESP_SLEEP_WAKEUP_EXT1 ? esp_sleep_get_ext1_wakeup_status() : 0;
  const bool coldBoot = cause == ESP_SLEEP_WAKEUP_UNDEFINED;

  uint32_t hold = 0;
  if ((wakePins & (1ULL << KEY_REFRESH)) || (coldBoot && digitalRead(KEY_REFRESH) == LOW))
    hold = keyHoldMs(KEY_REFRESH, RESET_PRESS_MS);
  if (hold >= RESET_PRESS_MS) {
    Serial.println("Factory reset");
    settingsFactoryReset();
    waitForButtonsReleased();
    ESP.restart();
  }

  batteryPct = readBatteryPercent();
  displayInit();

  // Drop forecasts of spots that were changed or removed
  const uint32_t hash = settingsSpotsHash(settings);
  if (cacheHash != hash) {
    for (Forecast& f : cache) f.valid = false;
    cacheHash = hash;
    nextUpdate = 0;
  }

  bool forceUpdate = false;
  if (!settingsHaveWifi(settings)) {
    setupFlow(nullptr, PORTAL_TIMEOUT_S);
    forceUpdate = true;
  } else if (hold >= LONG_PRESS_MS || settings.spotCount == 0) {
    if (connectWifi()) settingsFlow();
    else setupFlow(noWifiText(), PORTAL_TIMEOUT_S);
    forceUpdate = true;
  }
  if (settingsSpotsHash(settings) != cacheHash) {
    for (Forecast& f : cache) f.valid = false;
    cacheHash = settingsSpotsHash(settings);
  }

  const int pageCount = max(1, (settings.spotCount + SPOTS_PER_PAGE - 1) / SPOTS_PER_PAGE);
  int page = constrain(loadPage(), 0, pageCount - 1);
  const bool flip = pageCount > 1 && (wakePins & ((1ULL << KEY_NEXT) | (1ULL << KEY_PREV)));
  if (flip) {
    page = (wakePins & (1ULL << KEY_NEXT)) ? (page + 1) % pageCount : (page + pageCount - 1) % pageCount;
    savePage(page);
  }

  const int first = page * SPOTS_PER_PAGE;
  const int count = min(SPOTS_PER_PAGE, settings.spotCount - first);
  bool cached = true;
  for (int i = first; i < first + count; i++) cached &= cache[i].valid;

  // Paging only redraws from the cache, unless the next update is due anyway
  const time_t now = time(nullptr);
  const bool update = forceUpdate || !flip || !cached || !timeValid(now) || now >= nextUpdate;
  uint32_t sleepS;
  if (update) {
    if (updateAll()) {
      failCount = 0;
      nextUpdate = time(nullptr) + settings.intervalMin * 60;
      sleepS = settings.intervalMin * 60;
    } else {
      failCount++;
      sleepS = (failCount <= RETRY_COUNT ? RETRY_INTERVAL_MIN : settings.intervalMin) * 60;
      nextUpdate = time(nullptr) + sleepS;
    }
    wifiOff();
  } else {
    sleepS = max<int64_t>(60, nextUpdate - now);
  }

  const Spot* spots[SPOTS_PER_PAGE];
  const Forecast* fcs[SPOTS_PER_PAGE];
  int shown = 0;
  for (int i = first; i < first + count; i++) {
    if (!cache[i].valid) continue;
    spots[shown] = &settings.spots[i];
    fcs[shown++] = &cache[i];
  }
  if (shown > 0) {
    renderPage(settings, spots, fcs, shown, page, pageCount, failCount == 0, batteryPct, time(nullptr));
  } else {
    const Texts& tx = texts(settings.lang);
    renderError(settings, wifiFails > 0 ? noWifiText() : tx.apiFailed, batteryPct, sleepS / 60);
  }
  displaySleep();

  waitForButtonsReleased();
  deepSleepSeconds(sleepS);
}

void loop() {}
