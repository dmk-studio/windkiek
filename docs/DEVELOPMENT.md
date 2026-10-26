# Development

## Tools

- [PlatformIO](https://platformio.org) (VS Code extension or CLI), Arduino framework for ESP32
- Libraries: GxEPD2 (display driver), Adafruit GFX (drawing, fonts), ArduinoJson
- Python with Pillow for `tools/fontconv.py` (`pip install -r tools/requirements.txt`), only needed when fonts change
- Any static web server for the previews, e.g. `python -m http.server` in the project folder

Machine-specific PlatformIO settings (e.g. `core_dir`) go into `platformio_local.ini`, which is not under version control:

```ini
[platformio]
core_dir = D:/platformio-core
```

## Build and flash

```bash
pio run                 # build; also creates .pio/build/reterminal_e1001/windkiek.bin
pio run -t upload       # flash over USB-C (works while the device sleeps)
pio device monitor      # serial log, 115200 baud (only while the device is awake)
pio run -t erase        # erase the whole flash including all settings
```

Two build scripts run automatically:

- `tools/embed_web.py` (before the build) compresses `web/setup.html` into `src/web_page.h`
- `tools/merge_bin.py` (after the build) merges bootloader, partitions and app into `windkiek.bin`

## Flow

Every wakeup runs `setup()` in `src/main.cpp` from the top, then the device goes back to deep sleep:

1. Read keys: page change, middle key short / 3 s / 10 s.
2. No WiFi stored → setup WiFi (`runWifiPortal`), then settings page (`runSettingsPortal`).
3. Paging with fresh data only redraws from the RTC cache, no WiFi.
4. Otherwise: WiFi, NTP, fetch all spots, draw the current page.
5. Sleep until the next update (`intervalMin`) or 15 min after a failure (4 times, then the normal interval).

The device clock runs in UTC. Open-Meteo returns each spot's UTC offset (`timezone=auto`),
so dates, "today" and the update time are shown in the spot's local time.

## Where to change what

| Change | Place |
|---|---|
| Time slots (08/11/14/17/20) | `SLOT_HOURS` in `include/config.h`, same in `site/preview.js` |
| Max. spots, timeouts, retry interval | `include/config.h` |
| Layout (sizes, positions) | `src/render.cpp`, mirror the change in `site/preview.js` |
| Weather icons | `src/icons.h` and `site/preview.js` |
| Display texts / new language | `src/text.cpp`, `site/preview.js`, `web/setup.html`, `LANG_*` in `src/settings.h` |
| Settings page (phone) | `web/setup.html`; API in `src/portal.cpp` |
| New setting | `Settings` in `src/settings.h`, `toJson`/`fromJson` in `src/settings.cpp`, `web/setup.html` |
| Fonts | `FONTS` in `tools/fontconv.py`, then `python tools/fontconv.py src/fonts.h` |
| Weather model / API fields | URL in `src/forecast.cpp` and `site/preview.js` |
| Version | `FW_VERSION` in `include/config.h` (and `site/preview.js`), add a `CHANGELOG.md` entry |

## Working on layouts without the device

- `tools/preview.html`: all screens with live or demo data, language, unit, offline state.
  Start a web server in the project folder and open `http://localhost:8000/tools/preview.html`.
- `tools/mock_device.py [ap|sta]`: serves `web/setup.html` with a simulated device API on port 8766.

`site/preview.js` is a JavaScript copy of `src/render.cpp` with the same names and coordinates.
Change the layout there first, check it in the browser, then port it to C++.

## Settings page API

Served by the device on port 80 (setup WiFi: `4.3.2.1`, home WiFi: its IP or `windkiek.local`).

| Request | Purpose |
|---|---|
| `GET /` | The page (gzip) |
| `GET /api/state` | `{mode: "ap"}` or the settings plus `mode: "sta"` |
| `GET /api/scan` | WiFi networks (`?refresh=1` starts a new scan) |
| `POST /api/wifi` | `{ssid, pass, lang}`: try to join (setup WiFi only) |
| `GET /api/wifi` | Join state: `idle`, `connecting`, `ok`, `fail` |
| `POST /api/settings` | Save settings, the device then leaves the settings page |

Settings JSON:

```json
{
  "lang": "de", "unit": "kn", "interval": 120,
  "spots": [{ "name": "Mirns", "lat": 52.8491, "lon": 5.4846, "dirs": 8176, "min": 14, "sea": true }],
  "ssid": "MyWiFi", "pass": "only when changing"
}
```

`dirs` is a 16-bit mask: bit *i* = wind from sector *i* × 22.5° (bit 0 = N, bit 4 = E, bit 8 = S, bit 12 = W).
`min` is always in knots; the page converts it to the selected unit.

## Release

1. Raise `FW_VERSION` in `include/config.h` and `site/preview.js`, add a `CHANGELOG.md` entry.
2. Push to `main`: GitHub Actions builds the firmware and publishes `site/` with the firmware on GitHub Pages
   (one-time: repository settings → Pages → Source: GitHub Actions).
3. Tag `vX.Y.Z` and push the tag: the workflow attaches `windkiek.bin` to a GitHub release.

## Hardware (reTerminal E1001)

| Function | GPIO |
|---|---|
| ePaper SPI: SCK, MOSI, CS, DC, RST, BUSY | 7, 9, 10, 11, 12, 13 |
| Keys: right, middle, left (active low) | 3, 4, 5 |
| LED (active low) | 6 |
| Battery ADC, enable | 1, 21 (divider 2:1) |

The USB-C port goes through a CH340 to UART0, hence `ARDUINO_USB_CDC_ON_BOOT=0`.
Panel driver: `GxEPD2_750_GDEY075T7`.
