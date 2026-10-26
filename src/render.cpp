#include "render.h"

#include <GxEPD2_BW.h>
#include <SPI.h>
#include <qrcode.h>

#include "fonts.h"
#include "icons.h"
#include "text.h"

// Same layout as site/preview.js – try changes there first, then port them here.

// Full frame buffer (800x480/8 = 48 KB), no paging needed.
static GxEPD2_BW<GxEPD2_750_GDEY075T7, GxEPD2_750_GDEY075T7::HEIGHT> display(
    GxEPD2_750_GDEY075T7(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));
static SPIClass hspi(HSPI);

static const uint16_t BLACK = GxEPD_BLACK;
static const uint16_t WHITE = GxEPD_WHITE;

// Layout 800x480
static const int W = 800, H = 480;
static const int DAYBAR_H = 24, FOOTER_H = 16, PANEL_H = (H - DAYBAR_H - FOOTER_H) / 2;  // 220
static const int BAND_H = 22, HOUR_H = 16, ICON_H = 20, ARROW_H = 22;
static const int DAY_W = W / DAYS, BAR_W = 20;
static const float SLOT_W = (float)DAY_W / SLOTS;
static const int FOOTER_Y = H - FOOTER_H;

// Fonts (Inter, see tools/fontconv.py)
static const GFXfont* F_TITLE = &Inter800_28;
static const GFXfont* F_BODY = &Inter400_16;
static const GFXfont* F_TEXT = &Inter400_14;
static const GFXfont* F_NAME = &Inter800_16;
static const GFXfont* F_DAY = &Inter800_12;
static const GFXfont* F_DATE = &Inter400_12;
static const GFXfont* F_BAR = &Inter800_11;
static const GFXfont* F_GUST = &Inter600_11;
static const GFXfont* F_SMALL = &Inter400_10;
static const GFXfont* F_TINY = &Inter600_9;

void displayInit() {
  hspi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);
  display.epd2.selectSPI(hspi, SPISettings(2000000, MSBFIRST, SPI_MODE0));
  display.init(0);
  display.setRotation(0);
  display.setTextWrap(false);
}

void displaySleep() { display.hibernate(); }

// ---------- Text ----------

// UTF-8 -> font encoding (Latin-1, plus 0x7F = "–", 0x80 = "≥", 0x81-0x83 = „ “ ”)
static const char* encode(const char* s) {
  static char buf[160];
  size_t n = 0;
  const uint8_t* p = (const uint8_t*)s;
  while (*p && n < sizeof(buf) - 1) {
    if (p[0] < 0x80) {
      buf[n++] = *p++;
    } else if ((p[0] & 0xE0) == 0xC0 && p[1]) {  // 2 bytes: Latin-1 range
      buf[n++] = (char)(((p[0] & 0x1F) << 6) | (p[1] & 0x3F));
      p += 2;
    } else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
      uint16_t cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
      buf[n++] = cp == 0x2013 ? 0x7F : cp == 0x2265 ? 0x80 : cp == 0x201E ? 0x81
               : cp == 0x201C ? 0x82 : cp == 0x201D ? 0x83 : '?';
      p += 3;
    } else {
      buf[n++] = '?';
      p++;
    }
  }
  buf[n] = 0;
  return buf;
}

static int textWidth(const char* s, const GFXfont* f) {
  int w = 0;
  for (const uint8_t* p = (const uint8_t*)encode(s); *p; p++)
    if (*p >= f->first && *p <= f->last) w += f->glyph[*p - f->first].xAdvance;
  return w;
}

// Returns the x position after the text
static int text(int x, int y, const char* s, const GFXfont* f, uint16_t c) {
  const int w = textWidth(s, f);
  display.setFont(f);
  display.setTextColor(c);
  display.setCursor(x, y);
  display.print(encode(s));
  return x + w;
}

static void textC(int cx, int y, const char* s, const GFXfont* f, uint16_t c) {
  text(cx - textWidth(s, f) / 2, y, s, f, c);
}

static void textR(int rx, int y, const char* s, const GFXfont* f, uint16_t c) {
  text(rx - textWidth(s, f), y, s, f, c);
}

// Upper case for ASCII and Latin-1 letters in UTF-8 (ä = C3 A4 -> Ä = C3 84; ß stays)
static void upper(char* s) {
  for (uint8_t* p = (uint8_t*)s; *p; p++) {
    if (*p >= 'a' && *p <= 'z') *p -= 32;
    else if (*p == 0xC3 && p[1] >= 0xA0 && p[1] <= 0xBE && p[1] != 0xB7) *++p -= 0x20;
  }
}

// Shortens a UTF-8 string until it fits into maxW pixels
static void fitText(char* s, int maxW, const GFXfont* f) {
  size_t n = strlen(s);
  while (n > 0 && textWidth(s, f) > maxW) {
    do n--; while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80);  // keep UTF-8 sequences whole
    s[n] = 0;
    while (n > 0 && s[n - 1] == ' ') s[--n] = 0;
  }
}

// Word-wrapped text, returns the y of the next line
static int paragraph(int x, int y, int width, int lineH, const char* s, const GFXfont* f, uint16_t c) {
  char line[160] = "";
  char word[80];
  const char* p = s;
  while (*p) {
    while (*p == ' ') p++;
    int n = 0;
    while (p[n] && p[n] != ' ' && n < (int)sizeof(word) - 1) n++;
    if (!n) break;
    memcpy(word, p, n);
    word[n] = 0;
    p += n;
    char test[160];
    snprintf(test, sizeof(test), "%s%s%s", line, line[0] ? " " : "", word);
    if (line[0] && textWidth(test, f) > width) {
      text(x, y, line, f, c);
      y += lineH;
      strlcpy(line, word, sizeof(line));
    } else {
      strlcpy(line, test, sizeof(line));
    }
  }
  if (line[0]) {
    text(x, y, line, f, c);
    y += lineH;
  }
  return y;
}

// ---------- Graphics ----------

static void thickLine(int x1, int y1, int x2, int y2, int r, uint16_t c) {
  int n = max(abs(x2 - x1), abs(y2 - y1));
  for (int i = 0; i <= n; i++) {
    float t = n ? (float)i / n : 0;
    display.fillCircle(lroundf(x1 + (x2 - x1) * t), lroundf(y1 + (y2 - y1) * t), r, c);
  }
}

// Navigation arrow outline pointing where the wind blows: tip, two wings, notch at the back.
static void arrow(int cx, int cy, int fromDeg, uint16_t c, float len) {
  float r = (fromDeg + 180) * DEG_TO_RAD;
  float dx = sinf(r), dy = -cosf(r), px = -dy, py = dx;
  float half = len / 2, wing = len * 0.4f, notch = len * 0.3f;
  const float pts[4][2] = {
      {cx + dx * half, cy + dy * half},                          // tip
      {cx - dx * half + px * wing, cy - dy * half + py * wing},  // wing
      {cx - dx * (half - notch), cy - dy * (half - notch)},      // notch
      {cx - dx * half - px * wing, cy - dy * half - py * wing},  // wing
  };
  for (int i = 0; i < 4; i++) {
    const float* a = pts[i];
    const float* b = pts[(i + 1) % 4];
    display.drawLine(lroundf(a[0]), lroundf(a[1]), lroundf(b[0]), lroundf(b[1]), c);
  }
}

static void battery(int x, int y, int pct) {
  display.drawRect(x, y, 22, 10, BLACK);
  display.fillRect(x + 22, y + 3, 2, 4, BLACK);
  char buf[8];
  if (pct >= 0) {
    display.fillRect(x + 2, y + 2, 18 * pct / 100, 6, BLACK);
    snprintf(buf, sizeof(buf), "%d%%", pct);
  } else {
    snprintf(buf, sizeof(buf), "USB");
  }
  textR(x - 4, y + 9, buf, F_SMALL, BLACK);
}

static const uint8_t* qrData;  // set by the esp_qrcode display callback
static uint8_t qrBuf[1000];

static void qrCopy(esp_qrcode_handle_t q) {
  // The handle is only valid inside the callback, so copy the modules out.
  const int size = esp_qrcode_get_size(q);
  qrBuf[0] = size;
  for (int y = 0; y < size; y++)
    for (int x = 0; x < size; x++) {
      const int i = y * size + x;
      if (esp_qrcode_get_module(q, x, y)) qrBuf[1 + i / 8] |= 0x80 >> (i % 8);
      else qrBuf[1 + i / 8] &= ~(0x80 >> (i % 8));
    }
  qrData = qrBuf;
}

// QR code with its top left corner at x/y, returns the side length in pixels
static int qr(int x, int y, int module, const char* content) {
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.display_func = qrCopy;
  cfg.max_qrcode_version = 8;  // up to 49x49 modules, fits into qrBuf
  cfg.qrcode_ecc_level = ESP_QRCODE_ECC_MED;
  qrData = nullptr;
  if (esp_qrcode_generate(&cfg, content) != ESP_OK || !qrData) return 0;
  const int size = qrData[0];
  for (int my = 0; my < size; my++)
    for (int mx = 0; mx < size; mx++) {
      const int i = my * size + mx;
      if (qrData[1 + i / 8] & (0x80 >> (i % 8)))
        display.fillRect(x + mx * module, y + my * module, module, module, BLACK);
    }
  return size * module;
}

// ---------- Logic ----------

static bool dirOk(const Spot& spot, int dir) {
  const int sector = (int)lroundf(((dir % 360) + 360) % 360 / 22.5f) % 16;
  return spot.dirs & (1 << sector);
}

static bool isGood(const Spot& spot, const Slot& v) { return v.wind >= spot.minKn && dirOk(spot, v.dir); }

// Local time at the spot
static tm spotTime(const Forecast& fc, time_t t) {
  tm out;
  t += fc.utcOffset;
  gmtime_r(&t, &out);
  return out;
}

static bool isToday(const Forecast& fc, const tm& now) {
  return now.tm_year > 120 && now.tm_year + 1900 == fc.year && now.tm_mon + 1 == fc.month &&
         now.tm_mday == fc.day;
}

static tm dayDate(const Forecast& fc, int d) {
  tm t = {};
  t.tm_year = fc.year - 1900;
  t.tm_mon = fc.month - 1;
  t.tm_mday = fc.day + d;
  t.tm_hour = 12;
  mktime(&t);  // normalizes the date and sets the weekday (TZ is UTC)
  return t;
}

static void formatWind(char* buf, size_t len, float kn, Unit unit) {
  snprintf(buf, len, "%d", windValue(kn, unit));
}

// ---------- Sections ----------

static void dayBar(const Texts& tx, const Forecast& fc, int d0, bool today) {
  for (int d = 0; d < DAYS; d++) {
    const int x0 = d * DAY_W, fd = d0 + d;
    tm t = dayDate(fc, fd);
    if (d > 0) display.drawFastVLine(x0, 0, DAYBAR_H, BLACK);
    text(x0 + 8, 17, fd == 0 && today ? tx.today : tx.weekdays[t.tm_wday], F_DAY, BLACK);
    char date[12];
    snprintf(date, sizeof(date), "%02d.%02d.", t.tm_mday, t.tm_mon + 1);
    textR(x0 + DAY_W - 8, 17, date, F_DATE, BLACK);
  }
  display.fillRect(0, DAYBAR_H - 2, W, 2, BLACK);
}

struct GustPt {
  bool used, label;
  int x, y;
  float gust;
};

static void panel(int y0, int h, const Settings& s, const Spot& spot, const Forecast& fc, int d0, const tm& now,
                  int scale) {
  const bool today = isToday(fc, now);

  // Band: name on the left, temperature per day on the right of each day column.
  // The name must not run into the temperature of the first day.
  display.fillRect(0, y0, W, BAND_H, BLACK);
  char temps[DAYS][16];
  for (int d = 0; d < DAYS; d++) {
    const DayTemp& t = fc.temp[d0 + d];
    snprintf(temps[d], sizeof(temps[d]), "%d–%d°", (int)lroundf(t.min), (int)lroundf(t.max));
    textR((d + 1) * DAY_W - 8, y0 + 15, temps[d], F_GUST, WHITE);
  }
  const int nameMax = DAY_W - 8 - textWidth(temps[0], F_GUST) - 12 - 8;
  char buf[48];
  snprintf(buf, sizeof(buf), "%s", spot.name);
  upper(buf);
  fitText(buf, nameMax, F_NAME);
  text(8, y0 + 16, buf, F_NAME, WHITE);

  const int chartH = h - BAND_H - HOUR_H - ICON_H - ARROW_H;
  const int yHour = y0 + BAND_H, yChart = yHour + HOUR_H, yIcon = yChart + chartH, yArrow = yIcon + ICON_H;
  const int plotTop = yChart + 14, plotBot = yChart + chartH - 2;
  auto yOf = [&](float kn) {
    return (int)lroundf(plotBot - (min(kn, (float)scale) / scale) * (plotBot - plotTop));
  };

  // Dashed threshold line
  const int yT = yOf(spot.minKn);
  for (int lx = 0; lx < W; lx += 6) display.drawFastHLine(lx, yT, 3, BLACK);

  GustPt pts[DAYS * SLOTS] = {};
  int n = 0;
  for (int d = 0; d < DAYS; d++) {
    const int fd = d0 + d, x0 = d * DAY_W;
    if (d > 0) display.drawFastVLine(x0, yHour, h - BAND_H, BLACK);
    int maxI = -1;
    for (int si = 0; si < SLOTS; si++, n++) {
      const Slot& v = fc.slot[fd][si];
      const int cx = lroundf(x0 + SLOT_W * si + SLOT_W / 2);
      snprintf(buf, sizeof(buf), "%02d", SLOT_HOURS[si]);
      textC(cx, yHour + 12, buf, F_SMALL, BLACK);
      const bool past = fd == 0 && today && now.tm_hour > SLOT_HOURS[si] + 1;
      if (past || !v.valid) continue;

      arrow(cx, yArrow + 11, v.dir, BLACK, 14);
      drawWeatherIcon(display, cx, yIcon + 9, v.code, BLACK);

      // Bar: wind. Black only if strength AND direction fit.
      const int yTop = min(yOf(v.wind), plotBot - 15), bx = cx - BAR_W / 2;
      const bool hi = isGood(spot, v);
      if (hi) {
        display.fillRect(bx, yTop, BAR_W, plotBot - yTop + 1, BLACK);
      } else {
        display.fillRect(bx, yTop, BAR_W, plotBot - yTop + 1, WHITE);
        display.drawRect(bx, yTop, BAR_W, plotBot - yTop + 1, BLACK);
      }
      formatWind(buf, sizeof(buf), v.wind, s.unit);
      textC(cx, yTop + 12, buf, F_BAR, hi ? WHITE : BLACK);

      pts[n] = {true, false, cx, min(yOf(v.gust), yTop - 5), v.gust};
      if (maxI < 0 || v.gust > pts[maxI].gust) maxI = n;
    }
    if (maxI >= 0) pts[maxI].label = true;
  }

  // Line: gusts (white halo so it stays visible over black bars)
  const int radius[2] = {3, 1};
  const uint16_t color[2] = {WHITE, BLACK};
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 1; i < n; i++)
      if (pts[i - 1].used && pts[i].used)
        thickLine(pts[i - 1].x, pts[i - 1].y, pts[i].x, pts[i].y, radius[pass], color[pass]);
    for (int i = 0; i < n; i++)
      if (pts[i].used) display.fillCircle(pts[i].x, pts[i].y, radius[pass] + (pts[i].label ? 2 : 1), color[pass]);
  }
  // Gust value only at the daily maximum
  for (int i = 0; i < n; i++) {
    if (!pts[i].used || !pts[i].label) continue;
    formatWind(buf, sizeof(buf), pts[i].gust, s.unit);
    textC(pts[i].x, pts[i].y - 8, buf, F_GUST, BLACK);
  }
  display.drawFastHLine(0, plotBot + 1, W, BLACK);
}

static void footer(const Settings& s, const Forecast* fc, bool online, int batteryPct, int scale, int page,
                   int pageCount) {
  const Texts& tx = texts(s.lang);
  display.drawFastHLine(0, FOOTER_Y, W, BLACK);
  const int fy = FOOTER_Y + 12;
  char buf[64];
  tm f = {};
  if (fc) f = spotTime(*fc, fc->fetched);
  if (fc && f.tm_year > 120) {
    snprintf(buf, sizeof(buf), "%s%s%s %s %02d.%02d. %02d:%02d", online ? "" : tx.offline, online ? "" : " · ",
             tx.updated, tx.weekdaysShort[f.tm_wday], f.tm_mday, f.tm_mon + 1, f.tm_hour, f.tm_min);
  } else {
    snprintf(buf, sizeof(buf), "%s", online ? "" : tx.offline);
  }
  text(8, fy, buf, online ? F_SMALL : F_TINY, BLACK);
  if (scale <= 0) {
    battery(W - 30, FOOTER_Y + 3, batteryPct);
    return;
  }

  // Legend, centered between the timestamp and the right part
  int x = 228;
  display.drawRect(x, fy - 8, 8, 9, BLACK);
  x = text(x + 12, fy, tx.wind, F_SMALL, BLACK) + 12;
  display.fillRect(x, fy - 8, 8, 9, BLACK);
  x = text(x + 12, fy, tx.rideable, F_SMALL, BLACK) + 12;
  thickLine(x, fy - 4, x + 14, fy - 4, 1, BLACK);
  display.fillCircle(x + 7, fy - 4, 2, BLACK);
  x = text(x + 19, fy, tx.gusts, F_SMALL, BLACK) + 12;
  arrow(x + 6, fy - 4, 270, BLACK, 10);
  text(x + 16, fy, tx.direction, F_SMALL, BLACK);

  int rx = W - 30 - 44;  // left of the battery percentage
  if (pageCount > 1) {
    for (int i = pageCount - 1; i >= 0; i--) {
      if (i == page) display.fillCircle(rx - 3, fy - 4, 3, BLACK);
      else display.drawCircle(rx - 3, fy - 4, 3, BLACK);
      rx -= 10;
    }
    rx -= 6;
  }
  textR(rx, fy, "open-meteo.com", F_TINY, BLACK);
  battery(W - 30, FOOTER_Y + 3, batteryPct);
}

// Common frame for the setup screens: black band with the device name, returns the content top
static int setupFrame(const char* subtitle) {
  display.setFullWindow();
  display.fillScreen(WHITE);
  display.fillRect(0, 0, W, 44, BLACK);
  text(24, 29, "WINDKIEK", F_NAME, WHITE);
  char buf[48];
  snprintf(buf, sizeof(buf), "%s · v%s", subtitle, FW_VERSION);
  textR(W - 24, 28, buf, F_DATE, WHITE);
  display.drawFastHLine(0, H - 28, W, BLACK);
  text(24, H - 10, "Wind forecast: open-meteo.com", F_SMALL, BLACK);
  return 44;
}

// ---------- Screens ----------

void renderPage(const Settings& s, const Spot* const* spots, const Forecast* const* fcs, int count, int page,
                int pageCount, bool online, int batteryPct, time_t nowUtc) {
  display.setFullWindow();
  display.fillScreen(WHITE);
  const Texts& tx = texts(s.lang);
  const tm now = spotTime(*fcs[0], nowUtc);

  // From the last time slot of the day on, start with tomorrow
  const bool today = isToday(*fcs[0], now);
  const int d0 = today && now.tm_hour >= SLOT_HOURS[SLOTS - 1] ? 1 : 0;

  // Shared scale so the bars of both spots are comparable
  int scale = 20;
  for (int i = 0; i < count; i++)
    for (int d = d0; d < d0 + DAYS; d++)
      for (int si = 0; si < SLOTS; si++)
        if (fcs[i]->slot[d][si].valid) scale = max(scale, (int)ceilf((fcs[i]->slot[d][si].gust + 1) / 5) * 5);

  dayBar(tx, *fcs[0], d0, today);
  // A single spot on the page uses the full height
  const int h = count == 1 ? PANEL_H * 2 : PANEL_H;
  for (int i = 0; i < count; i++)
    panel(DAYBAR_H + i * PANEL_H, h, s, *spots[i], *fcs[i], d0, spotTime(*fcs[i], nowUtc), scale);
  footer(s, fcs[0], online, batteryPct, scale, page, pageCount);
  display.display(false);
}

void renderError(const Settings& s, const char* message, int batteryPct, uint32_t retryMin) {
  const Texts& tx = texts(s.lang);
  display.setFullWindow();
  display.fillScreen(WHITE);
  textC(W / 2, 200, tx.noData, F_TITLE, BLACK);
  textC(W / 2, 240, message, F_BODY, BLACK);
  char buf[96];
  snprintf(buf, sizeof(buf), tx.retryIn, (unsigned long)retryMin);
  textC(W / 2, 266, buf, F_BODY, BLACK);
  textC(W / 2, 320, tx.holdForSettings, F_DATE, BLACK);
  footer(s, nullptr, false, batteryPct, 0, 0, 1);
  display.display(false);
}

// Number in a black circle, as used next to the QR codes and in the instructions
static void badge(int cx, int cy, int n) {
  display.fillCircle(cx, cy, 10, BLACK);
  char buf[4];
  snprintf(buf, sizeof(buf), "%d", n);
  textC(cx, cy + 5, buf, F_DAY, WHITE);
}

void renderSetupWifi(const Settings& s, const char* reason) {
  const int top = setupFrame("Setup");
  // QR code 1 joins the setup WiFi, QR code 2 opens the setup page if the phone does not do it by itself
  const int qrX = 40;
  int y = top + 24;
  const int size1 = qr(qrX, y, 6, "WIFI:T:nopass;S:" SETUP_AP_SSID ";;");
  y += size1 + 22;
  badge(qrX + 10, y - 5, 1);
  text(qrX + 28, y, "WLAN / WiFi", F_NAME, BLACK);
  text(qrX + 28, y + 18, SETUP_AP_SSID, F_DATE, BLACK);
  y += 36;
  y += qr(qrX, y, 4, SETUP_AP_URL "/") + 22;
  badge(qrX + 10, y - 5, 2);
  text(qrX + 28, y, SETUP_AP_URL, F_NAME, BLACK);

  // Instructions in all languages: the language is chosen during setup
  struct Block {
    const char* lang;
    const char* lines[3];
  };
  static const Block BLOCKS[] = {
      {"DEUTSCH",
       {"1. QR-Code 1 scannen und mit dem WLAN „" SETUP_AP_SSID "“ verbinden.",
        "2. Die Einrichtungsseite öffnet sich von selbst. Falls nicht: QR-Code 2 scannen oder " SETUP_AP_URL
        " öffnen.",
        "3. Eigenes WLAN wählen und das Passwort eingeben."}},
      {"ENGLISH",
       {"1. Scan QR code 1 and join the WiFi “" SETUP_AP_SSID "”.",
        "2. The setup page opens by itself. If not: scan QR code 2 or open " SETUP_AP_URL ".",
        "3. Choose your WiFi and enter its password."}},
      {"NEDERLANDS",
       {"1. Scan QR-code 1 en verbind met de wifi „" SETUP_AP_SSID "”.",
        "2. De instelpagina opent vanzelf. Zo niet: scan QR-code 2 of open " SETUP_AP_URL ".",
        "3. Kies je eigen wifi en voer het wachtwoord in."}},
  };
  const int tx0 = 280, tw = W - tx0 - 32;
  y = top + 40;
  if (reason) y = paragraph(tx0, y, tw, 20, reason, F_NAME, BLACK) + 10;
  for (const Block& b : BLOCKS) {
    text(tx0, y, b.lang, F_DAY, BLACK);
    y += 20;
    for (const char* line : b.lines) y = paragraph(tx0, y, tw, 19, line, F_TEXT, BLACK);
    y += 12;
  }
  display.display(false);
}

void renderSettingsInfo(const Settings& s, const char* ip, bool noSpots) {
  const Texts& tx = texts(s.lang);
  const int top = setupFrame(tx.settingsTitle);
  char url[40];
  snprintf(url, sizeof(url), "http://%s/", ip);
  const int qrX = 40, qrY = top + 60;
  qr(qrX, qrY, 8, url);

  const int tx0 = 340, tw = W - tx0 - 32;
  int y = top + 72;
  y = paragraph(tx0, y, tw, 34, noSpots ? tx.noSpots : tx.settingsTitle, F_TITLE, BLACK) + 10;
  y = paragraph(tx0, y, tw, 22, tx.settingsScan, F_BODY, BLACK) + 18;
  y = paragraph(tx0, y, tw, 22, tx.settingsOpen, F_BODY, BLACK) + 6;
  text(tx0, y, "http://" MDNS_NAME ".local", F_NAME, BLACK);
  y += 24;
  text(tx0, y, url, F_NAME, BLACK);
  y += 40;
  paragraph(tx0, y, tw, 17, tx.settingsTimeout, F_DATE, BLACK);
  display.display(false);
}

void renderMessage(const Settings& s, const char* title, const char* line) {
  setupFrame(texts(s.lang).settingsTitle);
  textC(W / 2, 220, title, F_TITLE, BLACK);
  if (line) textC(W / 2, 260, line, F_BODY, BLACK);
  display.display(false);
}
