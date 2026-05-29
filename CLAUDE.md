# FlightWall — CYD Edition

ESP32 flight tracker displaying live nearby aircraft on a CYD TFT and embedded web dashboard. OpenSky ADS-B state vectors (OAuth2) → AeroAPI enrichment → FlightWall CDN name lookup → cycling flight card. Current version: 1.3.0. **Canonical version macro: `FW_VERSION_STR` in [`src/config/Version.h`](src/config/Version.h)** — bumping it there propagates to the WebUI title/brand badge and the boot serial banner. Markdown docs (this file, README.md, CHANGELOG.md) stay manually-maintained.

---

## Hardware targets

| Environment    | Board            | Driver  | Resolution | TFT_BL |
|:---------------|:-----------------|:--------|:-----------|:-------|
| `cyd_320x240`  | ESP32-2432S028R  | ILI9341 | 320 × 240  | GPIO 21 |
| `cyd_480x320`  | ESP32-3248S035R  | ST7796  | 480 × 320  | GPIO 27 — **verify against your board revision** |

Standard CYD SPI pins apply to both (MOSI=13, MISO=12, SCLK=14, CS=15, DC=2, RST=-1, TOUCH_CS=33). **MISO=12** is required for `/api/screenshot` framebuffer readback — without it `TFT_eSPI::readRectRGB()` reads a floating bus and returns all `0xFF` (all-white BMP). GPIO 12 is an ESP32 strapping pin (MTDI); usually safe because the ILI9341 tri-states MISO at boot, but revert if a specific board pulls it high and bootloops. `SPI_READ_FREQUENCY` is held at 6.25 MHz for read reliability on the CYD's unterminated MISO trace.

---

## Architectural decisions

**`FlightInfo` carries live ADS-B state.** `FlightDataFetcher` copies `icao24`, observation timestamps, location, `distance_km`, `bearing_deg`, altitude, velocity, heading, vertical rate, ground status, squawk and position source from each accepted `StateVector` into `FlightInfo`. `CYDDisplay` and the dashboard read these directly; they do not come from AeroAPI. `enriched` is true only when a current AeroAPI match has populated route metadata.

**Last-good-data policy.** `main.cpp` holds `g_flights` globally and only replaces it when a fetch returns a non-empty list. The display is called every `loop()` tick regardless of fetch state. Do not blank `g_flights` on an empty fetch result.

**ADS-B fallback cards.** `FlightDataFetcher` copies live ADS-B state into a `FlightInfo` for every accepted state vector before deciding whether to call AeroAPI. The card is then gated by `isDisplayableCard()` (file-scope static): enriched cards are always pushed; otherwise the card is dropped if `ident == icao24` (no callsign — typically `7cf4b0`-style hex transponders) or `on_ground` is true (parked/taxiing aircraft AeroAPI cannot match to a live record). Airborne, validly-callsigned, unenriched cards (e.g. AeroAPI rate-limited) are still pushed. This keeps the original 0.9.0 "do not blank on AeroAPI failure" guarantee intact while preventing empty "GROUND 0km/h" cards from cycling on the TFT.

**Empty-state TFT message.** `main.cpp` tracks `g_emptyMessage`. When `fetchFlights` returns aircraft (`states` non-empty) but the displayability filter removed all of them, `g_flights` is cleared and `g_emptyMessage` is set to `"No active flights within Nkm"`. The loop's tail dispatches flights → message → "Searching..." in that order. When the current fetch observes only filterable traffic, last-good `g_flights` is intentionally dropped so the TFT does not show a flight from minutes ago while reality is "nothing interesting nearby". `displayMessage` / `showLoading` are now idempotent on `_lastStatusMessage` to avoid flicker when called every loop tick.

**CYDDisplay render guard.** `displayFlights()` computes a `renderKey` (composite of all visible fields including live metrics) and skips the SPI draw if nothing changed. Always call `resetRenderState()` when switching display content (clear, message, loading) so the next `displayFlights()` call forces a redraw.

**AeroAPI filter document.** `AeroAPIFetcher` uses `DeserializationOption::Filter` with a `StaticJsonDocument<768>` to limit parsed fields. `DynamicJsonDocument doc(16384)` is sized for responses with 15+ historical flights. Do not remove the filter or reduce the doc — unfiltered AeroAPI responses cause `NoMemory` parse failures on-device. Uses `http.getString()` (not `getStream()`) — `getStream()` on ESP32-Arduino-3.x with `WiFiClientSecure` silently delivered zero bytes to the ArduinoJson parser (`doc-mem=0`, `err=Ok`); `getString()` buffers the full body first and is reliable. With `doc=16384` and ≤30 KB responses the combined heap usage is well within available memory.

**`parseIso8601` timezone handling.** AeroAPI returns departure/arrival timestamps with timezone designators (for example `"2026-05-25T08:00:00+08:00"`). `parseIso8601` converts the calendar fields through `utcCalendarToEpoch()` and subtracts the `+HH:MM` / `-HH:MM` offset. It must not use local-time `mktime`: startup now calls `configTzTime()` for Australia/Sydney debug timestamps, and the display timezone must not alter route scheduling calculations.

**AeroAPI flight record selection.** `AeroAPIFetcher` scans the full `flights[]` array only after NTP is synchronized and selects a plausible live record: departed but not arrived, arrived within a 30-minute grace window, or a bounded departure with no known arrival time. It prioritizes active flights and returns false if all records are historical or future. Never reintroduce index-zero fallback: a live callsign commonly returns old legs, which must remain ADS-B-only rather than displaying false route data. In ArduinoJson v6, the filter's `[0]` index acts as a template applied to all array elements.

**Web dashboard data path.** `WebUIServer` receives pointers to `g_flights` and `CYDDisplay`, and `main.cpp` records each completed fetch into a volatile 50-entry activity ring via `recordFetch()`. `GET /api/live` serves the selected display card, **all** `g_flights` (not capped — cap was removed in v0.14.0), activity events, busy/phase state and a `next_fetch_in` countdown; `GET /api/logo` serves cached LittleFS images. The TFT-mirror flight-card render is browser-rendered deliberately (not a framebuffer readback) for low SPI/network cost — but the *map-card* mirror does fetch the cached map JPEG from `/api/mappreview` plus an SVG overlay, since browser-side recreation is not pixel-accurate. `GET /api/screenshot` is the framebuffer-readback path, used on demand only for documentation captures. `GET /api/config` exposes only `opensky_configured` / `aero_configured` boolean flags — never the stored credential strings. `POST /api/config` saves to NVS and sets `_pendingReboot`; `main.cpp` reads `shouldReboot()` and triggers `ESP.restart()` after a 400 ms millis delay so the response can flush.

**Slot-tracking parity.** `WebUIServer::onGetLive()` must mirror `CYDDisplay::displayFlights()` exactly when computing the current slot: `totalSlots = flights.size() + 1` (the +1 is the map card), `slotIdx = currentFlightIndex % totalSlots`. Emit `screen.kind = "map"` when `slotIdx == flights.size()`, otherwise `"flight"` with the flight detail. Do **not** apply `% flights.size()` — that collapses the map slot back to flight #0 and makes it invisible to the dashboard. The bug existed from v1.1.0 until v1.2.0.

**Countdown timer source-of-truth.** `_lastFetchMs` is tracked in `WebUIServer` (set alongside `_lastFetchEpoch` in `recordFetch`). `next_fetch_in` in `/api/live` is computed from `millis()` arithmetic, not epoch arithmetic, so the countdown is immune to NTP drift or browser/device clock skew. Sentinel `-1` means "no fetch yet" (startup grace); `0` means "due now / overdue".

**FlightWall CDN brand colour dead code.** `FlightWallFetcher::getAirlineData` still parses an optional `brand_color_hex` from the FlightWall CDN response, but as of May 2026 the CDN endpoint returns only `icao` and `display_name_full` — no colour field. `airline_color` therefore remains at the `FlightInfo` default (`0xFFFF` white) and `CYDDisplay` falls back to `UserConfiguration::COLOR_AIRLINE`. Do not assume runtime airline colours will ever differ from white. The cached JPEG logo is now the only per-airline visual cue.

**Arduino ESP32 3.x LEDC API.** `CYDDisplay::initialize()` branches on `ESP_ARDUINO_VERSION_MAJOR >= 3`: uses `ledcAttachChannel(pin, freq, bits, channel)` + `ledcWrite(pin, duty)` for 3.x, and the old `ledcSetup()` + `ledcAttachPin()` + `ledcWrite(channel, duty)` for 2.x. Do not consolidate to one path.

**Font / metric layout.** Display fonts and layout constants are selected at compile time for each target resolution. Adjust the pixel offsets in `drawFlightCard` if you change fonts or add TFT fields.

---

## Library versions

- ArduinoJson pinned to **v6** (`^6.21.0`). All code uses `DynamicJsonDocument` / `StaticJsonDocument`. Do not upgrade to v7.
- TFT_eSPI `^2.4.76` — all pin/driver config via build flags only, never `User_Setup.h`.
- JPEG rendering: `Bodmer/TJpg_Decoder` (standalone lib — **not** bundled in TFT_eSPI 2.5.x). Include as `#include <TJpg_Decoder.h>`; the global object is still named `TJpgDec`.

---

## Credentials & secrets

`include/secrets.h` (gitignored) — copy from `include/secrets.h.template`. Pulled into `src/config/APIConfiguration.h` via `__has_include("secrets.h")`. Runtime API credential replacements are written from the dashboard to NVS; the configuration GET response exposes only configured flags, never stored secret text. WiFiManager manages WiFi credentials separately.

---

## Runtime configuration (RuntimeConfig)

`src/config/RuntimeConfig` is the single source of truth for all user-tuneable values at runtime. It is backed by NVS via `Preferences` (namespace `"flightwall"`). Load order: NVS value → compile-time default from `UserConfiguration` / `TimingConfiguration` / `HardwareConfiguration` / `APIConfiguration`.

NVS keys: `ctr_lat`, `ctr_lon`, `radius_km`, `fetch_sec`, `cycle_sec`, `map_sec`, `brightness`, `lbl_col`, `osky_id`, `osky_sec`, `aero_key`.

- `UserConfiguration.h`, `TimingConfiguration.h` compile-time constants remain as fallback defaults only — do not read them directly from fetchers or display code; use `RuntimeConfig::*()` instead.
- `APIConfiguration.h` still owns non-user-configurable values (URLs, TLS flags) — keep it for those; credentials come from `RuntimeConfig`.

## WebUI

`src/adapters/WebUIServer` runs `WebServer` on port 80. Routes: `GET /` (dashboard HTML), `GET /api/live` (all `g_flights` cards, activity feed, busy/phase state, JSON), `GET /api/logo` (cached JPEG), `GET /api/config` (non-sensitive runtime JSON including `label_color` as `#rrggbb`), `POST /api/config` (save to NVS and request reboot), `POST /api/fetchmap` (update centre/radius in memory and re-fetch map tile without NVS save), `GET /api/mappreview` (stream cached `mapcache.jpg` from LittleFS), `GET /api/screenshot` (24-bit BMP framebuffer dump via `TFT_eSPI::readRectRGB` — uses `readRectRGB` rather than `readRect` because the latter byte-swaps for `pushRect` compatibility). Credentials are write-only: blank POST credential values preserve existing data unless the matching clear flag is supplied. `main.cpp` polls `shouldReboot()` each loop tick and calls `ESP.restart()` after a 400 ms delay to allow TCP flush.

## Build notes

- All source lives under `src/`: `src/adapters/`, `src/config/`, `src/core/`, `src/interfaces/`, `src/models/`, `src/utils/`. Only `include/debug.h` and `include/secrets.h` sit outside `src/`. Headers use flat `#include "FileName.h"` — no subdirectory prefix — because every `src/` subdirectory has an explicit `-I` entry in `build_flags`.
- OpenSky and AeroAPI HTTP calls all set `http.setTimeout(15000)`. The default 5 s timeout produces `-11 HTTPC_ERROR_READ_TIMEOUT` on slow OpenSky `/states/all` responses. Do not drop these timeouts.
- `build_src_filter` paths are now relative to `src/` without `../` prefix (all `.cpp` files are inside `src/`). Add new `.cpp` files to the explicit list; do not use wildcards.
- Arduino ESP32 3.x does not auto-expose framework library headers — each one needs an explicit `-I $PROJECT_PACKAGES_DIR/framework-arduinoespressif32/libraries/{LibName}/src` entry in `[cyd_common]` build_flags. Current entries: `FS/src`, `LittleFS/src`, `Network/src`, `HTTPClient/src`, `NetworkClientSecure/src`, `WebServer/src`.
- `g_states` in `main.cpp` is retained for fetch-cycle telemetry supplied to `WebUIServer::recordFetch()`; the TFT renders from `g_flights`.

## Release process

When the user asks to push a release that has been merged to `main`:

1. Bump `FW_VERSION_STR` in `src/config/Version.h` to the clean version (e.g. `"1.3.0"`), update `README.md` and `CHANGELOG.md`, commit.
2. `git tag v1.3.0 && git push origin main --tags`

Pushing the tag triggers `.github/workflows/release.yml`, which builds both firmware targets and creates a GitHub Release with the CHANGELOG notes and both `.bin` files attached. The shields.io release badge on README.md updates automatically from the GitHub Releases API. No manual `gh release create` step needed.
