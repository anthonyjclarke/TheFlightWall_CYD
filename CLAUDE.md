# FlightWall — CYD Edition

ESP32 flight tracker displaying live nearby aircraft on a CYD TFT. OpenSky ADS-B state vectors (OAuth2) → AeroAPI enrichment → FlightWall CDN name lookup → cycling flight card. Current version: 0.12.0.

---

## Hardware targets

| Environment    | Board            | Driver  | Resolution | TFT_BL |
|:---------------|:-----------------|:--------|:-----------|:-------|
| `cyd_320x240`  | ESP32-2432S028R  | ILI9341 | 320 × 240  | GPIO 21 |
| `cyd_480x320`  | ESP32-3248S035R  | ST7796  | 480 × 320  | GPIO 27 — **verify against your board revision** |

Standard CYD SPI pins apply to both (MOSI=13, SCLK=14, CS=15, DC=2, RST=-1, TOUCH_CS=33).

---

## Architectural decisions

**`FlightInfo` carries live ADS-B state.** `FlightDataFetcher` copies `distance_km`, `bearing_deg`, `baro_altitude_m`, `geo_altitude_m`, `velocity_mps`, `heading_deg`, `vertical_rate_mps`, `on_ground` from the matching `StateVector` into `FlightInfo`. `CYDDisplay` reads these directly — they do not come from AeroAPI.

**Last-good-data policy.** `main.cpp` holds `g_flights` globally and only replaces it when a fetch returns a non-empty list. The display is called every `loop()` tick regardless of fetch state. Do not blank `g_flights` on an empty fetch result.

**ADS-B fallback cards.** `FlightDataFetcher` always pushes a `FlightInfo` to `outFlights`, even when AeroAPI enrichment fails. ADS-B state (altitude, speed, heading, distance, bearing) is copied before the AeroAPI call. When AeroAPI fails, `info.ident` is the trimmed OpenSky callsign; route and status lines remain empty, which `CYDDisplay` handles gracefully.

**CYDDisplay render guard.** `displayFlights()` computes a `renderKey` (composite of all visible fields including live metrics) and skips the SPI draw if nothing changed. Always call `resetRenderState()` when switching display content (clear, message, loading) so the next `displayFlights()` call forces a redraw.

**AeroAPI filter document.** `AeroAPIFetcher` uses `DeserializationOption::Filter` with a `StaticJsonDocument<768>` to limit parsed fields. `DynamicJsonDocument doc(16384)` is sized for responses with 15+ historical flights. Do not remove the filter or reduce the doc — unfiltered AeroAPI responses cause `NoMemory` parse failures on-device. Uses `http.getString()` (not `getStream()`) — `getStream()` on ESP32-Arduino-3.x with `WiFiClientSecure` silently delivered zero bytes to the ArduinoJson parser (`doc-mem=0`, `err=Ok`); `getString()` buffers the full body first and is reliable. With `doc=16384` and ≤30 KB responses the combined heap usage is well within available memory.

**`parseIso8601` timezone handling.** AeroAPI returns departure/arrival timestamps in the local timezone of the origin/destination airport (e.g. `"2026-05-25T08:00:00+08:00"` for a CST departure). `parseIso8601` reads and subtracts the `+HH:MM` / `-HH:MM` designator to convert to a true UTC epoch. `mktime` on ESP32 with `configTime(0,0,...)` treats `struct tm` as UTC, so the subtraction is correct. Without this, international flights appear never to have departed (departure epoch is hours in the future) and show a wildly wrong arrival countdown.

**AeroAPI flight record selection.** `AeroAPIFetcher` scans the full `flights[]` array and selects the entry whose departure (`actual_out` → `scheduled_out`) is the most recent one still in the past. AeroAPI sorts descending by `scheduled_out`, so `flights[0]` may be a future scheduled departure (tomorrow's service) or a previously completed inbound leg rather than the currently-airborne flight. The selection loop requires NTP to be synced (`now > 1e9`); if not synced it falls back to index 0. In ArduinoJson v6, the filter's `[0]` index acts as a template applied to all array elements — `doc["flights"]` contains all entries, not just the first.

**Arduino ESP32 3.x LEDC API.** `CYDDisplay::initialize()` branches on `ESP_ARDUINO_VERSION_MAJOR >= 3`: uses `ledcAttachChannel(pin, freq, bits, channel)` + `ledcWrite(pin, duty)` for 3.x, and the old `ledcSetup()` + `ledcAttachPin()` + `ledcWrite(channel, duty)` for 2.x. Do not consolidate to one path.

**Font / metric layout.** `TFT_HEIGHT >= 400` selects fonts and layout constants at compile time. `SHOW_TWO_METRIC_LINES` (a `constexpr bool` in `CYDDisplay.cpp`) is `true` for 480×320 (live summary + motion on separate rows) and `false` for 320×240 (compact single row). Adjust the pixel offsets in `drawFlightCard` if you change fonts.

---

## Library versions

- ArduinoJson pinned to **v6** (`^6.21.0`). All code uses `DynamicJsonDocument` / `StaticJsonDocument`. Do not upgrade to v7.
- TFT_eSPI `^2.4.76` — all pin/driver config via build flags only, never `User_Setup.h`.
- JPEG rendering: `Bodmer/TJpg_Decoder` (standalone lib — **not** bundled in TFT_eSPI 2.5.x). Include as `#include <TJpg_Decoder.h>`; the global object is still named `TJpgDec`.

---

---

## Credentials & secrets

`include/secrets.h` (gitignored) — copy from `include/secrets.h.template`. Pulled into `config/APIConfiguration.h` via `__has_include("secrets.h")`. WiFiManager stores WiFi credentials in NVS under its own namespace; no manual `Preferences` usage currently.

---

## Runtime configuration (RuntimeConfig)

`config/RuntimeConfig` is the single source of truth for all user-tuneable values at runtime. It is backed by NVS via `Preferences` (namespace `"flightwall"`). Load order: NVS value → compile-time default from `UserConfiguration` / `TimingConfiguration` / `HardwareConfiguration` / `APIConfiguration`.

NVS keys: `ctr_lat`, `ctr_lon`, `radius_km`, `fetch_sec`, `cycle_sec`, `brightness`, `osky_id`, `osky_sec`, `aero_key`.

- `UserConfiguration.h`, `TimingConfiguration.h` compile-time constants remain as fallback defaults only — do not read them directly from fetchers or display code; use `RuntimeConfig::*()` instead.
- `APIConfiguration.h` still owns non-user-configurable values (URLs, TLS flags) — keep it for those; credentials come from `RuntimeConfig`.

## WebUI

`adapters/WebUIServer` runs `WebServer` on port 80. Three routes: `GET /` (HTML page), `GET /api/config` (JSON), `POST /api/config` (JSON, saves to NVS, sets reboot flag). `main.cpp` polls `shouldReboot()` each loop tick and calls `ESP.restart()` after a 400 ms millis-based delay (allows TCP flush). `WebServer.h` is built into the ESP32 Arduino framework — no extra lib_dep, but requires the explicit `-I WebServer/src` path in build_flags.

## Build notes

- All source lives under `src/`: `src/adapters/`, `src/config/`, `src/core/`, `src/interfaces/`, `src/models/`, `src/utils/`. Only `include/debug.h` and `include/secrets.h` sit outside `src/`. Headers use flat `#include "FileName.h"` — no subdirectory prefix — because every `src/` subdirectory has an explicit `-I` entry in `build_flags`.
- `build_src_filter` paths are now relative to `src/` without `../` prefix (all `.cpp` files are inside `src/`). Add new `.cpp` files to the explicit list; do not use wildcards.
- Arduino ESP32 3.x does not auto-expose framework library headers — each one needs an explicit `-I $PROJECT_PACKAGES_DIR/framework-arduinoespressif32/libraries/{LibName}/src` entry in `[cyd_common]` build_flags. Current entries: `FS/src`, `LittleFS/src`, `Network/src`, `HTTPClient/src`, `NetworkClientSecure/src`, `WebServer/src`.
- `g_states` in `main.cpp` is retained but not passed to the display; reserved for a future range/bearing overlay.
