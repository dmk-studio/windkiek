#pragma once
#include <stdint.h>

// Fixed parameters of the firmware. Everything the user can change (WiFi, spots,
// language, unit, update interval) lives in settings.h and is edited on the setup page.

#define FW_VERSION "1.0.0"
#define FW_NAME "Windkiek"

// ---------- Forecast ----------
static const int MAX_SPOTS = 6;
static const int SPOTS_PER_PAGE = 2;
static const int DAYS = 4;        // days shown
static const int FETCH_DAYS = 5;  // one extra so the view can start tomorrow in the evening
static const int SLOT_HOURS[] = {8, 11, 14, 17, 20};  // local time at the spot
static const int SLOTS = sizeof(SLOT_HOURS) / sizeof(SLOT_HOURS[0]);

// ---------- Timing ----------
static const uint32_t RETRY_INTERVAL_MIN = 15;  // after a failed update
static const int RETRY_COUNT = 4;               // fast retries before falling back to the normal interval
static const int WIFI_FAILS_FOR_PORTAL = 3;     // consecutive WiFi failures that open the setup WiFi
static const uint32_t PORTAL_TIMEOUT_S = 600;   // setup WiFi / settings page stay open this long when idle
static const uint32_t PORTAL_AUTO_TIMEOUT_S = 180;  // ... when opened automatically after WiFi failures
static const uint32_t LONG_PRESS_MS = 3000;     // middle key: open settings
static const uint32_t RESET_PRESS_MS = 10000;   // middle key: factory reset

#define NTP_SERVER "pool.ntp.org"
#define SETUP_AP_SSID "Windkiek-Setup"
// Address of the device in its setup WiFi. Not the ESP32 default 192.168.4.1: some Android phones
// only open the sign-in page automatically for a public-looking address, and it is shorter to type.
#define SETUP_AP_IP 4, 3, 2, 1
#define SETUP_AP_URL "http://4.3.2.1"
#define MDNS_NAME "windkiek"  // http://windkiek.local

// ---------- Hardware reTerminal E1001 ----------
#define EPD_SCK_PIN 7
#define EPD_MOSI_PIN 9
#define EPD_CS_PIN 10
#define EPD_DC_PIN 11
#define EPD_RES_PIN 12
#define EPD_BUSY_PIN 13

#define KEY_NEXT 3     // KEY0, right (green)
#define KEY_REFRESH 4  // KEY1, middle
#define KEY_PREV 5     // KEY2, left

#define LED_PIN 6  // active low

#define BATTERY_ADC_PIN 1
#define BATTERY_EN_PIN 21
