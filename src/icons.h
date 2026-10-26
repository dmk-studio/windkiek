#pragma once
#include <Adafruit_GFX.h>

// Small weather icons (~18 px) drawn from graphics primitives, centered on cx/cy.
// Outline icons: black shape, then the inside (inset by 1 px) filled white again.

static uint16_t inverse(uint16_t c) { return c == GxEPD_BLACK ? GxEPD_WHITE : GxEPD_BLACK; }

static void cloudShape(Adafruit_GFX& g, int cx, int cy, int k, uint16_t c) {
  g.fillCircle(cx - 5, cy + 1, 4 - k, c);
  g.fillCircle(cx + 1, cy - 2, 5 - k, c);
  g.fillCircle(cx + 6, cy + 1, 4 - k, c);
  g.fillRect(cx - 5, cy + 1, 11, 5 - k, c);
}

static void iconCloud(Adafruit_GFX& g, int cx, int cy, uint16_t c) {
  cloudShape(g, cx, cy, 0, c);
  cloudShape(g, cx, cy, 1, inverse(c));
}

static void iconSun(Adafruit_GFX& g, int cx, int cy, int r, uint16_t c) {
  g.fillCircle(cx, cy, r, c);
  g.fillCircle(cx, cy, r - 1, inverse(c));
  for (int a = 0; a < 360; a += 45) {
    float rad = a * DEG_TO_RAD;
    g.drawLine(lroundf(cx + cosf(rad) * (r + 2)), lroundf(cy + sinf(rad) * (r + 2)),
               lroundf(cx + cosf(rad) * (r + 4)), lroundf(cy + sinf(rad) * (r + 4)), c);
  }
}

// WMO weather code -> icon
static void drawWeatherIcon(Adafruit_GFX& g, int cx, int cy, int code, uint16_t c) {
  if (code <= 1) {                        // clear / mainly clear
    iconSun(g, cx, cy, 4, c);
  } else if (code == 2) {                 // partly cloudy
    iconSun(g, cx - 4, cy - 3, 3, c);
    iconCloud(g, cx + 2, cy + 2, c);
  } else if (code == 3) {                 // overcast
    iconCloud(g, cx, cy, c);
  } else if (code == 45 || code == 48) {  // fog
    for (int i = 0; i < 3; i++) g.drawFastHLine(cx - 7 + (i % 2) * 2, cy - 4 + i * 4, 14, c);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {  // snow
    iconCloud(g, cx, cy - 3, c);
    for (int i = -1; i <= 1; i++) g.fillRect(cx + i * 5 - 1, cy + 6, 2, 2, c);
  } else if (code >= 95) {                // thunderstorm
    iconCloud(g, cx, cy - 3, c);
    g.drawLine(cx + 1, cy + 3, cx - 2, cy + 7, c);
    g.drawLine(cx - 2, cy + 7, cx + 2, cy + 7, c);
    g.drawLine(cx + 2, cy + 7, cx - 1, cy + 11, c);
  } else {                                // drizzle, rain, showers
    iconCloud(g, cx, cy - 3, c);
    for (int i = -1; i <= 1; i++) g.drawLine(cx + i * 5 + 1, cy + 4, cx + i * 5 - 1, cy + 8, c);
  }
}
