# FlightWall — CYD Edition

ESP32 flight tracker displaying live nearby aircraft on a CYD TFT and embedded web dashboard. OpenSky ADS-B state vectors (OAuth2) → AeroAPI enrichment → FlightWall CDN name lookup → cycling flight card. Current version: 0.13.0.

---

## Hardware targets

| Environment    | Board            | Driver  | Resolution | TFT_BL |
|:---------------|:-----------------|:--------|:-----------|:-------|
| `cyd_320x240`  | ESP32-2432S028R  | ILI9341 | 320 × 240  | GPIO 21 |
| `cyd_480x320`  | ESP32-3248S035R  | ST7796  | 480 × 320  | GPIO 27 — **verify against your board revision** |

Standard CYD SPI pins apply to both (MOSI=13, SCLK=14, CS=15, DC=2, RST=-1, TOUCH_CS=33).

---

## Architectural decisions

**`FlightInfo` carries live ADS-B state.** `FlightDataFetcher` copies `icao24`, observation timestamps, location, `distance_km`, `bearing_deg`, altitude, velocity, heading, vertical rate, ground status, squawk and position source from each accepted `StateVector` into `FlightInfo`. `CYDDisplay` and the dashboard read these directly; they do not come from AeroAPI. `enriched` is true only when a current AeroAPI match has populated route metadata.

**Last-good-data policy.** `main.cpp` holds `g_flights` globally and only replaces it when a fetch returns a non-empty list. The display is called every `loop()` tick regardless of fetch state. Do not blank `g_flights` on an empty fetch result.

**ADS-B fallback cards.** `FlightDataFetcher` always pushes a `FlightInfo` to `outFlights`, even when callsign validation fails, the AeroAPI call allowance has been used, or enrichment returns no current record. ADS-B state is copied before any enrichment attempt. If the callsign is absent, `info.ident` falls back to `icao24`; route and status lines remain empty, which `CYDDisplay` handles gracefully.

**CYDDisplay render guard.** `displayFlights()` computes a `renderKey` (composite of all visible fields including live metrics) and skips the SPI draw if nothing changed. Always call `resetRenderState()` when switching display content (clear, message, loading) so the next `displayFlights()` call forces a redraw.

**AeroAPI filter document.** `AeroAPIFetcher` uses `DeserializationOption::Filter` with a `StaticJsonDocument<768>` to limit parsed fields. `DynamicJsonDocument doc(16384)` is sized for responses with 15+ historical flights. Do not remove the filter or reduce the doc — unfiltered AeroAPI responses cause `NoMemory` parse failures on-device. Uses `http.getString()` (not `getStream()`) — `getStream()` on ESP32-Arduino-3.x with `WiFiClientSecure` silently delivered zero bytes to the ArduinoJson parser (`doc-mem=0`, `err=Ok`); `getString()` buffers the full body first and is reliable. With `doc=16384` and ≤30 KB responses the combined heap usage is well within available memory.

**`parseIso8601` timezone handling.** AeroAPI returns departure/arrival timestamps with timezone designators (for example `"2026-05-25T08:00:00+08:00"`). `parseIso8601` converts the calendar fields through `utcCalendarToEpoch()` and subtracts the `+HH:MM` / `-HH:MM` offset. It must not use local-time `mktime`: startup now calls `configTzTime()` for Australia/Sydney debug timestamps, and the display timezone must not alter route scheduling calculations.

**AeroAPI flight record selection.** `AeroAPIFetcher` scans the full `flights[]` array only after NTP is synchronized and selects a plausible live record: departed but not arrived, arrived within a 30-minute grace window, or a bounded departure with no known arrival time. It prioritizes active flights and returns false if all records are historical or future. Never reintroduce index-zero fallback: a live callsign commonly returns old legs, which must remain ADS-B-only rather than displaying false route data. In ArduinoJson v6, the filter's `[0]` index acts as a template applied to all array elements.

**Web dashboard data path.** `WebUIServer` receives pointers to `g_flights` and `CYDDisplay`, and `main.cpp` records each completed fetch into a volatile 50-entry activity ring. `GET /api/live` serves the selected display card, activity events and at most five `enriched` detail records; `GET /api/logo` serves cached LittleFS images. The TFT mirror is browser-rendered deliberately, not a framebuffer pixel readback, to avoid extra SPI and transfer cost on the ESP32.

**Arduino ESP32 3.x LEDC API.** `CYDDisplay::initialize()` branches on `ESP_ARDUINO_VERSION_MAJOR >= 3`: uses `ledcAttachChannel(pin, freq, bits, channel)` + `ledcWrite(pin, duty)` for 3.x, and the old `ledcSetup()` + `ledcAttachPin()` + `ledcWrite(channel, duty)` for 2.x. Do not consolidate to one path.

**Font / metric layout.** Display fonts and layout constants are selected at compile time for each target resolution. Adjust the pixel offsets in `drawFlightCard` if you change fonts or add TFT fields.

---

## Library versions

- ArduinoJson pinned to **v6** (`^6.21.0`). All code uses `DynamicJsonDocument` / `StaticJsonDocument`. Do not upgrade to v7.
- TFT_eSPI `^2.4.76` — all pin/driver config via build flags only, never `User_Setup.h`.
- JPEG rendering: `Bodmer/TJpg_Decoder` (standalone lib — **not** bundled in TFT_eSPI 2.5.x). Include as `#include <TJpg_Decoder.h>`; the global object is still named `TJpgDec`.

---

---

## Credentials & secrets

`include/secrets.h` (gitignored) — copy from `include/secrets.h.template`. Pulled into `src/config/APIConfiguration.h` via `__has_include("secrets.h")`. Runtime API credential replacements are written from the dashboard to NVS; the configuration GET response exposes only configured flags, never stored secret text. WiFiManager manages WiFi credentials separately.

---

## Runtime configuration (RuntimeConfig)

`src/config/RuntimeConfig` is the single source of truth for all user-tuneable values at runtime. It is backed by NVS via `Preferences` (namespace `"flightwall"`). Load order: NVS value → compile-time default from `UserConfiguration` / `TimingConfiguration` / `HardwareConfiguration` / `APIConfiguration`.

NVS keys: `ctr_lat`, `ctr_lon`, `radius_km`, `fetch_sec`, `cycle_sec`, `brightness`, `osky_id`, `osky_sec`, `aero_key`.

- `UserConfiguration.h`, `TimingConfiguration.h` compile-time constants remain as fallback defaults only — do not read them directly from fetchers or display code; use `RuntimeConfig::*()` instead.
- `APIConfiguration.h` still owns non-user-configurable values (URLs, TLS flags) — keep it for those; credentials come from `RuntimeConfig`.

## WebUI

`src/adapters/WebUIServer` runs `WebServer` on port 80. Routes: `GET /` (dashboard HTML), `GET /api/live` (screen, enriched records and activity JSON), `GET /api/logo` (cached JPEG), `GET /api/config` (non-sensitive runtime JSON), and `POST /api/config` (save to NVS and request reboot). Credentials are write-only: blank POST credential values preserve existing data unless the matching clear flag is supplied. `main.cpp` polls `shouldReboot()` each loop tick and calls `ESP.restart()` after a 400 ms delay to allow TCP flush.

## Build notes

- All source lives under `src/`: `src/adapters/`, `src/config/`, `src/core/`, `src/interfaces/`, `src/models/`, `src/utils/`. Only `include/debug.h` and `include/secrets.h` sit outside `src/`. Headers use flat `#include "FileName.h"` — no subdirectory prefix — because every `src/` subdirectory has an explicit `-I` entry in `build_flags`.
- `build_src_filter` paths are now relative to `src/` without `../` prefix (all `.cpp` files are inside `src/`). Add new `.cpp` files to the explicit list; do not use wildcards.
- Arduino ESP32 3.x does not auto-expose framework library headers — each one needs an explicit `-I $PROJECT_PACKAGES_DIR/framework-arduinoespressif32/libraries/{LibName}/src` entry in `[cyd_common]` build_flags. Current entries: `FS/src`, `LittleFS/src`, `Network/src`, `HTTPClient/src`, `NetworkClientSecure/src`, `WebServer/src`.
- `g_states` in `main.cpp` is retained for fetch-cycle telemetry supplied to `WebUIServer::recordFetch()`; the TFT renders from `g_flights`.
