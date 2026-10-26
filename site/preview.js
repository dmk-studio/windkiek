// Browser copy of src/render.cpp: draws the ePaper screens on an 800x480 canvas.
// Used by tools/preview.html (layout development) and the website (site/index.html).
// Keep both in sync: same coordinates, same function names.

import qrcode from "./vendor/qrcode.mjs";

// ---------- config.h ----------
export const DAYS = 4, FETCH_DAYS = 5, SLOT_HOURS = [8, 11, 14, 17, 20], SLOTS = SLOT_HOURS.length;
export const SPOTS_PER_PAGE = 2, MAX_SPOTS = 6;
export const FW_VERSION = "1.0.0";
const SETUP_AP_SSID = "Windkiek-Setup", SETUP_AP_URL = "http://4.3.2.1", MDNS_NAME = "windkiek";

// ---------- text.cpp ----------
export const TEXTS = {
  de: {
    weekdays: ["SONNTAG", "MONTAG", "DIENSTAG", "MITTWOCH", "DONNERSTAG", "FREITAG", "SAMSTAG"],
    weekdaysShort: ["So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"],
    today: "HEUTE", updated: "Stand", offline: "OFFLINE", wind: "Wind", rideable: "fahrbar", gusts: "Böen",
    direction: "Windrichtung", noData: "Keine Forecast-Daten", noWifi: "Keine Verbindung zum WLAN „%s“",
    apiFailed: "Abruf bei Open-Meteo fehlgeschlagen", retryIn: "Neuer Versuch in %d Minuten",
    holdForSettings: "Einstellungen: mittlere Taste 3 Sekunden halten", settingsTitle: "Einstellungen",
    settingsScan: "QR-Code mit dem Handy scannen (im selben WLAN)", settingsOpen: "oder im Browser öffnen:",
    settingsTimeout: "Die Seite schließt nach 10 Minuten ohne Eingabe. Mittlere Taste: beenden.",
    noSpots: "Noch keine Spots eingerichtet", setupClosed: "Einrichtung beendet", pressToRestart: "Zum Fortsetzen eine Taste drücken",
  },
  en: {
    weekdays: ["SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"],
    weekdaysShort: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"],
    today: "TODAY", updated: "Updated", offline: "OFFLINE", wind: "Wind", rideable: "rideable", gusts: "Gusts",
    direction: "Wind direction", noData: "No forecast data", noWifi: "Cannot connect to WiFi “%s”",
    apiFailed: "Open-Meteo request failed", retryIn: "Next try in %d minutes",
    holdForSettings: "Settings: hold the middle key for 3 seconds", settingsTitle: "Settings",
    settingsScan: "Scan the QR code with your phone (same WiFi)", settingsOpen: "or open in a browser:",
    settingsTimeout: "The page closes after 10 minutes without input. Middle key: exit.",
    noSpots: "No spots set up yet", setupClosed: "Setup closed", pressToRestart: "Press any key to continue",
  },
  nl: {
    weekdays: ["ZONDAG", "MAANDAG", "DINSDAG", "WOENSDAG", "DONDERDAG", "VRIJDAG", "ZATERDAG"],
    weekdaysShort: ["zo", "ma", "di", "wo", "do", "vr", "za"],
    today: "VANDAAG", updated: "Bijgewerkt", offline: "OFFLINE", wind: "Wind", rideable: "geschikt", gusts: "Windstoten",
    direction: "Windrichting", noData: "Geen verwachting beschikbaar", noWifi: "Geen verbinding met wifi „%s”",
    apiFailed: "Ophalen bij Open-Meteo mislukt", retryIn: "Nieuwe poging over %d minuten",
    holdForSettings: "Instellingen: middelste knop 3 seconden ingedrukt houden", settingsTitle: "Instellingen",
    settingsScan: "Scan de QR-code met je telefoon (zelfde wifi)", settingsOpen: "of open in de browser:",
    settingsTimeout: "De pagina sluit na 10 minuten zonder invoer. Middelste knop: stoppen.",
    noSpots: "Nog geen spots ingesteld", setupClosed: "Instellen beëindigd", pressToRestart: "Druk op een knop om verder te gaan",
  },
};

const BFT_LIMITS = [1, 4, 7, 11, 17, 22, 28, 34, 41, 48, 56, 64];
const beaufort = kn => { let b = 0; while (b < 12 && kn >= BFT_LIMITS[b] - 0.5) b++; return b; };
const knToUnit = (kn, u) => u === "ms" ? kn * 0.514444 : u === "kmh" ? kn * 1.852 : u === "bft" ? beaufort(kn) : kn;
const windValue = (kn, u) => Math.round(knToUnit(kn, u));

// ---------- render.cpp ----------
const W = 800, H = 480;
const DAYBAR_H = 24, FOOTER_H = 16, PANEL_H = (H - DAYBAR_H - FOOTER_H) / 2;  // 220
const BAND_H = 22, HOUR_H = 16, ICON_H = 20, ARROW_H = 22;
const DAY_W = W / DAYS, SLOT_W = DAY_W / SLOTS, BAR_W = 20;
const FOOTER_Y = H - FOOTER_H;
const F = {
  title: "800 28px Inter", body: "400 16px Inter", text: "400 14px Inter", name: "800 16px Inter", day: "800 12px Inter", date: "400 12px Inter",
  bar: "800 11px Inter", gust: "600 11px Inter", small: "400 10px Inter", tiny: "600 9px Inter",
};
const BLACK = "#000", WHITE = "#fff";
const pad = n => String(n).padStart(2, "0");

export function createScreen(canvas) {
  const g = canvas.getContext("2d");

  // Text: returns the x position after the text
  function text(x, y, s, font, c) { g.font = font; g.fillStyle = c; g.textBaseline = "alphabetic"; g.fillText(s, x, y); return x + tw(s, font); }
  function tw(s, font) { g.font = font; return Math.round(g.measureText(s).width); }
  const textC = (cx, y, s, f, c) => text(cx - tw(s, f) / 2, y, s, f, c);
  const textR = (rx, y, s, f, c) => text(rx - tw(s, f), y, s, f, c);
  function paragraph(x, y, width, lineH, s, f, c) {
    let line = "";
    for (const word of s.split(" ").filter(Boolean)) {
      const test = line ? line + " " + word : word;
      if (line && tw(test, f) > width) { text(x, y, line, f, c); y += lineH; line = word; } else line = test;
    }
    if (line) { text(x, y, line, f, c); y += lineH; }
    return y;
  }

  // Graphics
  function fillRect(x, y, w, h, c) { g.fillStyle = c; g.fillRect(x, y, w, h); }
  function drawRect(x, y, w, h, c) { fillRect(x, y, w, 1, c); fillRect(x, y + h - 1, w, 1, c); fillRect(x, y, 1, h, c); fillRect(x + w - 1, y, 1, h, c); }
  const hline = (x, y, w, c) => fillRect(x, y, w, 1, c);
  const vline = (x, y, h, c) => fillRect(x, y, 1, h, c);
  function circle(x, y, r, c) { g.fillStyle = c; g.beginPath(); g.arc(x + .5, y + .5, r + .5, 0, 7); g.fill(); }
  function ring(x, y, r, c) { g.strokeStyle = c; g.lineWidth = 1; g.beginPath(); g.arc(x + .5, y + .5, r, 0, 7); g.stroke(); }
  function line(x1, y1, x2, y2, c) { g.strokeStyle = c; g.lineWidth = 1; g.beginPath(); g.moveTo(x1 + .5, y1 + .5); g.lineTo(x2 + .5, y2 + .5); g.stroke(); }
  function thickLine(x1, y1, x2, y2, r, c) {
    const n = Math.max(Math.abs(x2 - x1), Math.abs(y2 - y1));
    for (let i = 0; i <= n; i++) { const t = n ? i / n : 0; circle(Math.round(x1 + (x2 - x1) * t), Math.round(y1 + (y2 - y1) * t), r, c); }
  }

  // icons.h: outline icons, black shape with the inside filled white again
  function cloudShape(cx, cy, k, c) { circle(cx - 5, cy + 1, 4 - k, c); circle(cx + 1, cy - 2, 5 - k, c); circle(cx + 6, cy + 1, 4 - k, c); fillRect(cx - 5, cy + 1, 11, 5 - k, c); }
  function iconCloud(cx, cy, c) { cloudShape(cx, cy, 0, c); cloudShape(cx, cy, 1, c === BLACK ? WHITE : BLACK); }
  function iconSun(cx, cy, r, c) {
    circle(cx, cy, r, c); circle(cx, cy, r - 1, c === BLACK ? WHITE : BLACK);
    for (let a = 0; a < 360; a += 45) { const d = a * Math.PI / 180;
      line(Math.round(cx + Math.cos(d) * (r + 2)), Math.round(cy + Math.sin(d) * (r + 2)), Math.round(cx + Math.cos(d) * (r + 4)), Math.round(cy + Math.sin(d) * (r + 4)), c); }
  }
  function weatherIcon(cx, cy, code, c) {
    if (code <= 1) iconSun(cx, cy, 4, c);
    else if (code === 2) { iconSun(cx - 4, cy - 3, 3, c); iconCloud(cx + 2, cy + 2, c); }
    else if (code === 3) iconCloud(cx, cy, c);
    else if (code === 45 || code === 48) { for (let i = 0; i < 3; i++) hline(cx - 7 + (i % 2) * 2, cy - 4 + i * 4, 14, c); }
    else if ((code >= 71 && code <= 77) || code === 85 || code === 86) {
      iconCloud(cx, cy - 3, c); for (let i = -1; i <= 1; i++) fillRect(cx + i * 5 - 1, cy + 6, 2, 2, c);
    } else if (code >= 95) {
      iconCloud(cx, cy - 3, c); line(cx + 1, cy + 3, cx - 2, cy + 7, c); line(cx - 2, cy + 7, cx + 2, cy + 7, c); line(cx + 2, cy + 7, cx - 1, cy + 11, c);
    } else {
      iconCloud(cx, cy - 3, c); for (let i = -1; i <= 1; i++) line(cx + i * 5 + 1, cy + 4, cx + i * 5 - 1, cy + 8, c);
    }
  }

  // Navigation arrow outline pointing where the wind blows: tip, two wings, notch at the back.
  function arrow(cx, cy, fromDeg, c, len) {
    const r = (fromDeg + 180) * Math.PI / 180, dx = Math.sin(r), dy = -Math.cos(r), px = -dy, py = dx;
    const half = len / 2, wing = len * 0.4, notch = len * 0.3;
    const pts = [
      [cx + dx * half, cy + dy * half],
      [cx - dx * half + px * wing, cy - dy * half + py * wing],
      [cx - dx * (half - notch), cy - dy * (half - notch)],
      [cx - dx * half - px * wing, cy - dy * half - py * wing],
    ].map(([x, y]) => [Math.round(x), Math.round(y)]);
    for (let i = 0; i < 4; i++) { const a = pts[i], b = pts[(i + 1) % 4]; line(a[0], a[1], b[0], b[1], c); }
  }

  function battery(x, y, pct) {
    drawRect(x, y, 22, 10, BLACK); fillRect(x + 22, y + 3, 2, 4, BLACK);
    if (pct >= 0) fillRect(x + 2, y + 2, Math.floor(18 * pct / 100), 6, BLACK);
    textR(x - 4, y + 9, pct >= 0 ? pct + "%" : "USB", F.small, BLACK);
  }

  function qr(x, y, module, content) {
    const q = qrcode(0, "M");
    q.addData(content);
    q.make();
    const n = q.getModuleCount();
    for (let my = 0; my < n; my++) for (let mx = 0; mx < n; mx++) if (q.isDark(my, mx)) fillRect(x + mx * module, y + my * module, module, module, BLACK);
    return n * module;
  }

  // Logic
  const dirOk = (spot, dir) => (spot.dirs >> (Math.round((((dir % 360) + 360) % 360) / 22.5) % 16)) & 1;
  const isGood = (spot, v) => v.wind >= spot.min && dirOk(spot, v.dir);
  const spotTime = (fc, t) => new Date(t + fc.utcOffset * 1000);  // read with getUTC*
  const isToday = (fc, now) => now.getUTCFullYear() === fc.year && now.getUTCMonth() + 1 === fc.month && now.getUTCDate() === fc.day;
  const dayDate = (fc, d) => new Date(Date.UTC(fc.year, fc.month - 1, fc.day + d, 12));

  function dayBar(tx, fc, d0, today) {
    for (let d = 0; d < DAYS; d++) {
      const x0 = d * DAY_W, fd = d0 + d, t = dayDate(fc, fd);
      if (d > 0) vline(x0, 0, DAYBAR_H, BLACK);
      text(x0 + 8, 17, fd === 0 && today ? tx.today : tx.weekdays[t.getUTCDay()], F.day, BLACK);
      textR(x0 + DAY_W - 8, 17, `${pad(t.getUTCDate())}.${pad(t.getUTCMonth() + 1)}.`, F.date, BLACK);
    }
    fillRect(0, DAYBAR_H - 2, W, 2, BLACK);
  }

  function panel(y0, h, s, spot, fc, d0, now, scale) {
    const today = isToday(fc, now);
    // Band: name on the left, temperature per day on the right of each day column.
    // The name must not run into the temperature of the first day.
    fillRect(0, y0, W, BAND_H, BLACK);
    const temps = [];
    for (let d = 0; d < DAYS; d++) {
      const t = fc.temp[d0 + d];
      temps.push(`${Math.round(t.min)}–${Math.round(t.max)}°`);
      textR((d + 1) * DAY_W - 8, y0 + 15, temps[d], F.gust, WHITE);
    }
    const nameMax = DAY_W - 8 - tw(temps[0], F.gust) - 12 - 8;
    let name = spot.name.toUpperCase();
    while (name && tw(name, F.name) > nameMax) name = name.slice(0, -1).trimEnd();
    text(8, y0 + 16, name, F.name, WHITE);

    const chartH = h - BAND_H - HOUR_H - ICON_H - ARROW_H;
    const yHour = y0 + BAND_H, yChart = yHour + HOUR_H, yIcon = yChart + chartH, yArrow = yIcon + ICON_H;
    const plotTop = yChart + 14, plotBot = yChart + chartH - 2;
    const yOf = kn => Math.round(plotBot - (Math.min(kn, scale) / scale) * (plotBot - plotTop));

    // Dashed threshold line
    const yT = yOf(spot.min);
    for (let lx = 0; lx < W; lx += 6) hline(lx, yT, 3, BLACK);

    const pts = [];
    for (let d = 0; d < DAYS; d++) {
      const fd = d0 + d, x0 = d * DAY_W;
      if (d > 0) vline(x0, yHour, h - BAND_H, BLACK);
      let maxI = -1;
      for (let si = 0; si < SLOTS; si++) {
        const v = fc.slot[fd][si], cx = Math.round(x0 + SLOT_W * si + SLOT_W / 2);
        textC(cx, yHour + 12, pad(SLOT_HOURS[si]), F.small, BLACK);
        const past = fd === 0 && today && now.getUTCHours() > SLOT_HOURS[si] + 1;
        if (past || !v) { pts.push(null); continue; }
        arrow(cx, yArrow + 11, v.dir, BLACK, 14);
        weatherIcon(cx, yIcon + 9, v.code, BLACK);
        // Bar: wind. Black only if strength AND direction fit.
        const yTop = Math.min(yOf(v.wind), plotBot - 15), bx = cx - BAR_W / 2, hi = isGood(spot, v);
        if (hi) fillRect(bx, yTop, BAR_W, plotBot - yTop + 1, BLACK);
        else { fillRect(bx, yTop, BAR_W, plotBot - yTop + 1, WHITE); drawRect(bx, yTop, BAR_W, plotBot - yTop + 1, BLACK); }
        textC(cx, yTop + 12, String(windValue(v.wind, s.unit)), F.bar, hi ? WHITE : BLACK);
        pts.push({ x: cx, y: Math.min(yOf(v.gust), yTop - 5), gust: v.gust });
        if (maxI < 0 || v.gust > pts[maxI].gust) maxI = pts.length - 1;
      }
      if (maxI >= 0) pts[maxI].label = true;
    }
    // Line: gusts (white halo so it stays visible over black bars)
    for (const [r, c] of [[3, WHITE], [1, BLACK]]) {
      for (let i = 1; i < pts.length; i++) if (pts[i - 1] && pts[i]) thickLine(pts[i - 1].x, pts[i - 1].y, pts[i].x, pts[i].y, r, c);
      for (const p of pts) if (p) circle(p.x, p.y, r + (p.label ? 2 : 1), c);
    }
    // Gust value only at the daily maximum
    for (const p of pts) if (p && p.label) textC(p.x, p.y - 8, String(windValue(p.gust, s.unit)), F.gust, BLACK);
    hline(0, plotBot + 1, W, BLACK);
  }

  function footer(s, fc, online, batteryPct, scale, page, pageCount) {
    const tx = TEXTS[s.lang];
    hline(0, FOOTER_Y, W, BLACK);
    const fy = FOOTER_Y + 12;
    let stamp = online ? "" : tx.offline;
    if (fc) {
      const f = spotTime(fc, fc.fetched);
      stamp = `${online ? "" : tx.offline + " · "}${tx.updated} ${tx.weekdaysShort[f.getUTCDay()]} ${pad(f.getUTCDate())}.${pad(f.getUTCMonth() + 1)}. ${pad(f.getUTCHours())}:${pad(f.getUTCMinutes())}`;
    }
    text(8, fy, stamp, online ? F.small : F.tiny, BLACK);
    if (scale <= 0) { battery(W - 30, FOOTER_Y + 3, batteryPct); return; }

    let x = 228;
    drawRect(x, fy - 8, 8, 9, BLACK);
    x = text(x + 12, fy, tx.wind, F.small, BLACK) + 12;
    fillRect(x, fy - 8, 8, 9, BLACK);
    x = text(x + 12, fy, tx.rideable, F.small, BLACK) + 12;
    thickLine(x, fy - 4, x + 14, fy - 4, 1, BLACK); circle(x + 7, fy - 4, 2, BLACK);
    x = text(x + 19, fy, tx.gusts, F.small, BLACK) + 12;
    arrow(x + 6, fy - 4, 270, BLACK, 10);
    text(x + 16, fy, tx.direction, F.small, BLACK);

    let rx = W - 30 - 44;
    if (pageCount > 1) {
      for (let i = pageCount - 1; i >= 0; i--) {
        if (i === page) circle(rx - 3, fy - 4, 3, BLACK); else ring(rx - 3, fy - 4, 3, BLACK);
        rx -= 10;
      }
      rx -= 6;
    }
    textR(rx, fy, "open-meteo.com", F.tiny, BLACK);
    battery(W - 30, FOOTER_Y + 3, batteryPct);
  }

  function badge(cx, cy, n) { circle(cx, cy, 10, BLACK); textC(cx, cy + 5, String(n), F.day, WHITE); }

  function setupFrame(subtitle) {
    fillRect(0, 0, W, H, WHITE);
    fillRect(0, 0, W, 44, BLACK);
    text(24, 29, "WINDKIEK", F.name, WHITE);
    textR(W - 24, 28, `${subtitle} · v${FW_VERSION}`, F.date, WHITE);
    hline(0, H - 28, W, BLACK);
    text(24, H - 10, "Wind forecast: open-meteo.com", F.small, BLACK);
    return 44;
  }

  // Reduce to 1 bit like the ePaper
  function finish() {
    const img = g.getImageData(0, 0, W, H), p = img.data;
    for (let i = 0; i < p.length; i += 4) { const v = (p[i] + p[i + 1] + p[i + 2]) / 3 < 140 ? 0 : 255; p[i] = p[i + 1] = p[i + 2] = v; p[i + 3] = 255; }
    g.putImageData(img, 0, 0);
  }

  return {
    // spots/fcs: the spots of this page (1 or 2), nowUtc in ms
    page(s, spots, fcs, page, pageCount, online, batteryPct, nowUtc) {
      fillRect(0, 0, W, H, WHITE);
      const tx = TEXTS[s.lang];
      const now = spotTime(fcs[0], nowUtc);
      const today = isToday(fcs[0], now);
      const d0 = today && now.getUTCHours() >= SLOT_HOURS[SLOTS - 1] ? 1 : 0;
      let scale = 20;
      for (const fc of fcs) for (let d = d0; d < d0 + DAYS; d++) for (const v of fc.slot[d]) if (v) scale = Math.max(scale, Math.ceil((v.gust + 1) / 5) * 5);
      dayBar(tx, fcs[0], d0, today);
      const h = fcs.length === 1 ? PANEL_H * 2 : PANEL_H;
      fcs.forEach((fc, i) => panel(DAYBAR_H + i * PANEL_H, h, s, spots[i], fc, d0, spotTime(fc, nowUtc), scale));
      footer(s, fcs[0], online, batteryPct, scale, page, pageCount);
      finish();
    },
    error(s, message, batteryPct, retryMin) {
      const tx = TEXTS[s.lang];
      fillRect(0, 0, W, H, WHITE);
      textC(W / 2, 200, tx.noData, F.title, BLACK);
      textC(W / 2, 240, message, F.body, BLACK);
      textC(W / 2, 266, tx.retryIn.replace("%d", retryMin), F.body, BLACK);
      textC(W / 2, 320, tx.holdForSettings, F.date, BLACK);
      footer(s, null, false, batteryPct, 0, 0, 1);
      finish();
    },
    setupWifi(reason) {
      const top = setupFrame("Setup");
      // QR code 1 joins the setup WiFi, QR code 2 opens the setup page if the phone does not do it by itself
      const qrX = 40;
      let y = top + 24;
      y += qr(qrX, y, 6, `WIFI:T:nopass;S:${SETUP_AP_SSID};;`) + 22;
      badge(qrX + 10, y - 5, 1);
      text(qrX + 28, y, "WLAN / WiFi", F.name, BLACK);
      text(qrX + 28, y + 18, SETUP_AP_SSID, F.date, BLACK);
      y += 36;
      y += qr(qrX, y, 4, SETUP_AP_URL + "/") + 22;
      badge(qrX + 10, y - 5, 2);
      text(qrX + 28, y, SETUP_AP_URL, F.name, BLACK);

      const blocks = [
        ["DEUTSCH", [`1. QR-Code 1 scannen und mit dem WLAN „${SETUP_AP_SSID}“ verbinden.`,
          `2. Die Einrichtungsseite öffnet sich von selbst. Falls nicht: QR-Code 2 scannen oder ${SETUP_AP_URL} öffnen.`,
          "3. Eigenes WLAN wählen und das Passwort eingeben."]],
        ["ENGLISH", [`1. Scan QR code 1 and join the WiFi “${SETUP_AP_SSID}”.`,
          `2. The setup page opens by itself. If not: scan QR code 2 or open ${SETUP_AP_URL}.`,
          "3. Choose your WiFi and enter its password."]],
        ["NEDERLANDS", [`1. Scan QR-code 1 en verbind met de wifi „${SETUP_AP_SSID}”.`,
          `2. De instelpagina opent vanzelf. Zo niet: scan QR-code 2 of open ${SETUP_AP_URL}.`,
          "3. Kies je eigen wifi en voer het wachtwoord in."]],
      ];
      const tx0 = 280, tw0 = W - tx0 - 32;
      y = top + 40;
      if (reason) y = paragraph(tx0, y, tw0, 20, reason, F.name, BLACK) + 10;
      for (const [lang, lines] of blocks) {
        text(tx0, y, lang, F.day, BLACK);
        y += 20;
        for (const l of lines) y = paragraph(tx0, y, tw0, 19, l, F.text, BLACK);
        y += 12;
      }
      finish();
    },
    settingsInfo(s, ip, noSpots) {
      const tx = TEXTS[s.lang];
      const top = setupFrame(tx.settingsTitle);
      const url = `http://${ip}/`;
      qr(40, top + 60, 8, url);
      const tx0 = 340, tw0 = W - tx0 - 32;
      let y = top + 72;
      y = paragraph(tx0, y, tw0, 34, noSpots ? tx.noSpots : tx.settingsTitle, F.title, BLACK) + 10;
      y = paragraph(tx0, y, tw0, 22, tx.settingsScan, F.body, BLACK) + 18;
      y = paragraph(tx0, y, tw0, 22, tx.settingsOpen, F.body, BLACK) + 6;
      text(tx0, y, `http://${MDNS_NAME}.local`, F.name, BLACK);
      y += 24;
      text(tx0, y, url, F.name, BLACK);
      y += 40;
      paragraph(tx0, y, tw0, 17, tx.settingsTimeout, F.date, BLACK);
      finish();
    },
  };
}

// ---------- forecast.cpp ----------
export function parseForecast(j) {
  const h = j.hourly, [year, month, day] = h.time[0].slice(0, 10).split("-").map(Number);
  const slot = [], temp = [];
  for (let d = 0; d < FETCH_DAYS; d++) {
    const ts = h.temperature_2m.slice(d * 24 + 7, d * 24 + 22).filter(v => v != null);  // daytime 07-21 h
    temp.push({ min: Math.min(...ts), max: Math.max(...ts) });
    slot.push(SLOT_HOURS.map(hr => {
      const i = d * 24 + hr;
      if (h.wind_speed_10m[i] == null || h.wind_gusts_10m[i] == null || h.wind_direction_10m[i] == null) return null;
      let code = 0;
      for (let k = Math.max(0, i - 1); k <= Math.min(FETCH_DAYS * 24 - 1, i + 1); k++) code = Math.max(code, h.weather_code[k] || 0);
      return { wind: h.wind_speed_10m[i], gust: h.wind_gusts_10m[i], dir: h.wind_direction_10m[i], code };
    }));
  }
  return { year, month, day, utcOffset: j.utc_offset_seconds || 0, fetched: Date.now(), slot, temp };
}

export async function fetchForecast(spot) {
  const url = `https://api.open-meteo.com/v1/forecast?latitude=${spot.lat}&longitude=${spot.lon}` +
    `&hourly=wind_speed_10m,wind_gusts_10m,wind_direction_10m,weather_code,temperature_2m` +
    `&wind_speed_unit=kn&timezone=auto&forecast_days=${FETCH_DAYS}&models=best_match&cell_selection=${spot.sea ? "sea" : "land"}`;
  const res = await fetch(url);
  if (!res.ok) throw new Error("HTTP " + res.status);
  return parseForecast(await res.json());
}

// Plausible made-up forecast (deterministic per seed), starting today in the browser's time zone
export function demoForecast(seed, nowMs = Date.now()) {
  const utcOffset = -new Date(nowMs).getTimezoneOffset() * 60;
  const local = new Date(nowMs + utcOffset * 1000);
  const rnd = (i) => { const x = Math.sin(seed * 97.3 + i * 12.9898) * 43758.5453; return x - Math.floor(x); };
  const codes = [0, 1, 2, 2, 3, 3, 61, 80, 2, 1];
  const slot = [], temp = [];
  for (let d = 0; d < FETCH_DAYS; d++) {
    const base = 9 + 8 * Math.sin((d + seed) * 1.3) + rnd(d) * 4;
    const dirBase = (220 + d * 35 + seed * 40) % 360;
    slot.push(SLOT_HOURS.map((hr, s) => {
      const wind = Math.max(3, Math.round(base + 4 * Math.sin(s * 0.9 + d) + rnd(d * 7 + s) * 2));
      return { wind, gust: Math.round(wind * (1.3 + rnd(d * 3 + s) * 0.25)), dir: Math.round(dirBase + s * 8 + rnd(s + d) * 20) % 360,
        code: codes[Math.floor(rnd(d * 11 + s + seed) * codes.length)] };
    }));
    const t = 13 + 4 * Math.sin(d + seed) + rnd(d + 50) * 2;
    temp.push({ min: t - 4 - rnd(d) * 2, max: t + 2 });
  }
  return { year: local.getUTCFullYear(), month: local.getUTCMonth() + 1, day: local.getUTCDate(), utcOffset,
    fetched: nowMs - 25 * 60000, slot, temp };
}

export async function loadFonts() {
  await Promise.all(["800 28px Inter", "400 16px Inter", "400 14px Inter", "800 16px Inter", "400 10px Inter", "600 11px Inter", "800 11px Inter", "400 12px Inter", "600 9px Inter", "800 12px Inter"]
    .map(f => document.fonts.load(f)));
}
