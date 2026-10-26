#pragma once
#include "settings.h"

// Setup through the phone: the device serves web/setup.html and a small JSON API.

enum PortalResult { PORTAL_SAVED, PORTAL_TIMEOUT, PORTAL_EXIT };

// Opens the setup WiFi (captive portal) to enter the home WiFi. Returns true once the device is
// connected to it; the WiFi is then saved in s and NVS. reason: optional line for the display.
bool runWifiPortal(Settings& s, uint32_t timeoutS, const char* reason);

// Settings page while connected to the home WiFi (http://windkiek.local or the IP on the display).
PortalResult runSettingsPortal(Settings& s, uint32_t timeoutS);
