#include "text.h"

#include <math.h>

static const Texts DE = {
    {"SONNTAG", "MONTAG", "DIENSTAG", "MITTWOCH", "DONNERSTAG", "FREITAG", "SAMSTAG"},
    {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"},
    "HEUTE",
    "Stand",
    "OFFLINE",
    "Wind",
    "fahrbar",
    "Böen",
    "Windrichtung",
    "Keine Forecast-Daten",
    "Keine Verbindung zum WLAN „%s“",
    "Abruf bei Open-Meteo fehlgeschlagen",
    "Neuer Versuch in %lu Minuten",
    "Einstellungen: mittlere Taste 3 Sekunden halten",
    "Einstellungen",
    "QR-Code mit dem Handy scannen (im selben WLAN)",
    "oder im Browser öffnen:",
    "Die Seite schließt nach 10 Minuten ohne Eingabe. Mittlere Taste: beenden.",
    "Noch keine Spots eingerichtet",
    "Einrichtung beendet",
    "Zum Fortsetzen eine Taste drücken",
};

static const Texts EN = {
    {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"},
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
    "TODAY",
    "Updated",
    "OFFLINE",
    "Wind",
    "rideable",
    "Gusts",
    "Wind direction",
    "No forecast data",
    "Cannot connect to WiFi “%s”",
    "Open-Meteo request failed",
    "Next try in %lu minutes",
    "Settings: hold the middle key for 3 seconds",
    "Settings",
    "Scan the QR code with your phone (same WiFi)",
    "or open in a browser:",
    "The page closes after 10 minutes without input. Middle key: exit.",
    "No spots set up yet",
    "Setup closed",
    "Press any key to continue",
};

static const Texts NL = {
    {"ZONDAG", "MAANDAG", "DINSDAG", "WOENSDAG", "DONDERDAG", "VRIJDAG", "ZATERDAG"},
    {"zo", "ma", "di", "wo", "do", "vr", "za"},
    "VANDAAG",
    "Bijgewerkt",
    "OFFLINE",
    "Wind",
    "geschikt",
    "Windstoten",
    "Windrichting",
    "Geen verwachting beschikbaar",
    "Geen verbinding met wifi „%s”",
    "Ophalen bij Open-Meteo mislukt",
    "Nieuwe poging over %lu minuten",
    "Instellingen: middelste knop 3 seconden ingedrukt houden",
    "Instellingen",
    "Scan de QR-code met je telefoon (zelfde wifi)",
    "of open in de browser:",
    "De pagina sluit na 10 minuten zonder invoer. Middelste knop: stoppen.",
    "Nog geen spots ingesteld",
    "Instellen beëindigd",
    "Druk op een knop om verder te gaan",
};

const Texts& texts(Lang lang) {
  switch (lang) {
    case LANG_DE: return DE;
    case LANG_NL: return NL;
    default: return EN;
  }
}

static int beaufort(float kn) {
  // Upper limits in knots for force 0..11
  static const float LIMITS[] = {1, 4, 7, 11, 17, 22, 28, 34, 41, 48, 56, 64};
  int b = 0;
  while (b < 12 && kn >= LIMITS[b] - 0.5f) b++;
  return b;
}

float knToUnit(float kn, Unit unit) {
  switch (unit) {
    case UNIT_MS: return kn * 0.514444f;
    case UNIT_KMH: return kn * 1.852f;
    case UNIT_BFT: return beaufort(kn);
    default: return kn;
  }
}

int windValue(float kn, Unit unit) { return (int)lroundf(knToUnit(kn, unit)); }

const char* unitLabel(Unit unit) {
  switch (unit) {
    case UNIT_MS: return "m/s";
    case UNIT_KMH: return "km/h";
    case UNIT_BFT: return "Bft";
    default: return "kn";
  }
}
