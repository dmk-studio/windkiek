# Windkiek für das reTerminal E1001

[English](README.md)

Kostenlose Open-Source-Firmware, die das **Seeed reTerminal E1001** (ESP32-S3, 7,5"-Schwarz-Weiß-ePaper)
zur Windvorhersage für Kite-, Windsurf- und Segelspots macht.
*Kieken* ist plattdeutsch für gucken: Windkiek ist der schnelle Blick auf den Wind.

- 4 Tage, alle 3 Stunden von 08 bis 20 Uhr: Wind als Balken, Böen als Linie, Wetter, Richtung, Temperatur
- **Schwarzer Balken** = Mindestwind erreicht **und** Wind aus einer fahrbaren Richtung (pro Spot einstellbar)
- Bis zu 6 Spots, je zwei pro Seite, Tasten links/rechts blättern
- Einrichtung am Handy: keine App, kein Konto, kein Programmieren
- Sprachen DE / EN / NL, Einheiten kn, m/s, km/h, Bft
- Akkubetrieb: Tiefschlaf zwischen den Updates (alle 1–4 Std.)
- Wetterdaten von [Open-Meteo.com](https://open-meteo.com) (CC BY 4.0), Modell „Best Match“

**Website und Web-Installer:** https://dmk-studio.github.io/windkiek/

## Installation

### Im Browser (empfohlen)

1. Website in Chrome oder Edge am Computer öffnen.
2. reTerminal E1001 mit einem USB-C-Datenkabel anschließen.
3. Auf **Installieren** klicken und den Port wählen (meist „USB-SERIAL CH340“).

Die Original-Firmware (SenseCraft HMI) wird dabei überschrieben. Wie man sie zurückbekommt, beschreibt Seeed in der
[SenseCraft-HMI-Dokumentation](https://sensecraft-hmi-docs.seeed.cc/en/guides/firmware-update).

### Mit PlatformIO

```bash
pio run -t upload
```

## Einrichtung

1. Nach der Installation zeigt das Display einen QR-Code. Scannen oder mit dem WLAN `Windkiek-Setup` verbinden.
2. Auf der Seite, die sich öffnet, das eigene WLAN (2,4 GHz) wählen und das Passwort eingeben.
3. Das Display verbindet sich und zeigt einen zweiten QR-Code. Handy wieder ins eigene WLAN, Code scannen
   (oder `http://windkiek.local` öffnen).
4. Spots anlegen: Ort suchen oder auf der Karte setzen, fahrbare Richtungen auf der Kompassrose antippen,
   Mindestwind wählen, bei Spots am Meer „Küstenspot“ ankreuzen. Speichern.

| Taste | Funktion |
|---|---|
| Links / rechts | Vorherige / nächste Seite |
| Mitte | Sofort aktualisieren |
| Mitte 3 s halten | Einstellungsseite (LED leuchtet) |
| Mitte 10 s halten | Werkseinstellungen (LED blinkt) |

Ist das WLAN dreimal hintereinander nicht erreichbar, öffnet das Display für 3 Minuten das Einrichtungs-WLAN.

## Aufbau des Projekts

| Pfad | Inhalt |
|---|---|
| `src/main.cpp` | Ablauf pro Aufwachen: Tasten, Einrichtung, WLAN, Update, Zeichnen, Tiefschlaf |
| `src/render.cpp` | Layout aller Bildschirme |
| `src/icons.h` | Wetter-Icons |
| `src/forecast.cpp` | Abruf und Auswertung bei Open-Meteo |
| `src/settings.cpp` | Einstellungen im Flash (NVS), JSON für die Einstellungsseite |
| `src/portal.cpp` | Einrichtungs-WLAN, Captive Portal und Server der Einstellungsseite |
| `src/text.cpp` | Display-Texte DE/EN/NL, Umrechnung der Einheiten |
| `src/power.cpp` | Akku, LED, Tasten, Tiefschlaf |
| `src/fonts.h` | Schrift Inter als 1-Bit-Fonts, erzeugt mit `tools/fontconv.py` |
| `include/config.h` | Feste Parameter: Version, Zeitpunkte, Timeouts, Pins |
| `web/setup.html` | Einstellungsseite fürs Handy, wird beim Build in die Firmware eingebettet |
| `site/` | Website mit Vorschau und Web-Installer (GitHub Pages) |
| `tools/preview.html` | Layout-Vorschau im Browser mit Live-Daten |
| `tools/mock_device.py` | Simuliert das Gerät, um an `web/setup.html` zu arbeiten |

Mehr Details: [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) (englisch)

## Lizenz

- Code: [GPL-3.0](LICENSE)
- Schrift Inter: SIL Open Font License 1.1 ([tools/fonts/OFL.txt](tools/fonts/OFL.txt))
- Wetterdaten: [Open-Meteo.com](https://open-meteo.com), CC BY 4.0

Unabhängiges Projekt, nicht mit Seeed Studio verbunden. reTerminal ist eine Marke von Seeed Technology.
Die Software wird ohne Gewährleistung bereitgestellt; Nutzung auf eigene Gefahr.
