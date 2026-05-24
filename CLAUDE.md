# FlightWall — CYD Edition

ESP32 flight tracker displaying live nearby aircraft on a CYD TFT. OpenSky ADS-B state vectors (OAuth2) → AeroAPI enrichment → FlightWall CDN name lookup → cycling flight card. Current version: 0.3.0.

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

**CYDDisplay render guard.** `displayFlights()` computes a `renderKey` (composite of all visible fields including live metrics) and skips the SPI draw if nothing changed. Always call `resetRenderState()` when switching display content (clear, message, loading) so the next `displayFlights()` call forces a redraw.

**AeroAPI filter document.** `AeroAPIFetcher` uses `DeserializationOption::Filter` with a `StaticJsonDocument<512>` to limit parsed fields. `DynamicJsonDocument` is capped at 4096 bytes. Do not remove the filter — unfiltered AeroAPI responses are too large and cause `NoMemory` parse failures on-device.

**Arduino ESP32 3.x LEDC API.** `CYDDisplay::initialize()` branches on `ESP_ARDUINO_VERSION_MAJOR >= 3`: uses `ledcAttachChannel(pin, freq, bits, channel)` + `ledcWrite(pin, duty)` for 3.x, and the old `ledcSetup()` + `ledcAttachPin()` + `ledcWrite(channel, duty)` for 2.x. Do not consolidate to one path.

**Font / metric layout.** `TFT_HEIGHT >= 400` selects fonts and layout constants at compile time. `SHOW_TWO_METRIC_LINES` (a `constexpr bool` in `CYDDisplay.cpp`) is `true` for 480×320 (live summary + motion on separate rows) and `false` for 320×240 (compact single row). Adjust the pixel offsets in `drawFlightCard` if you change fonts.

---

## Library versions

- ArduinoJson pinned to **v6** (`^6.21.0`). All code uses `DynamicJsonDocument` / `StaticJsonDocument`. Do not upgrade to v7.
- TFT_eSPI `^2.4.76` — all pin/driver config via build flags only, never `User_Setup.h`.

---

## Build notes

- `build_src_filter` paths are relative to `src/` — `../adapters/` resolves to project-root `adapters/`. `NeoMatrixDisplay.cpp` is intentionally absent from the filter; do not add it.
- Arduino ESP32 3.x requires explicit `-I` paths for `Network.h`, `HTTPClient.h`, `NetworkClientSecure.h` — already in `[cyd_common]` build_flags.
- `g_states` in `main.cpp` is retained but not passed to the display; reserved for a future range/bearing overlay in Phase 2.

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
