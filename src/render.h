#pragma once
#include <time.h>
#include "forecast.h"
#include "settings.h"

void displayInit();
void displaySleep();

// One page: one or two spots below each other. spots[i] belongs to fcs[i].
void renderPage(const Settings& s, const Spot* const* spots, const Forecast* const* fcs, int count, int page,
                int pageCount, bool online, int batteryPct, time_t now);
void renderError(const Settings& s, const char* message, int batteryPct, uint32_t retryMin);
// Setup WiFi is open. reason: optional line on top (e.g. "cannot connect to ..."), in the device language.
void renderSetupWifi(const Settings& s, const char* reason);
// Settings page is reachable at http://<ip>/
void renderSettingsInfo(const Settings& s, const char* ip, bool noSpots);
void renderMessage(const Settings& s, const char* title, const char* line);
