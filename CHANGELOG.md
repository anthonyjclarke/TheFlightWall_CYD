# Changelog

---

## Todo

### Features
- **Visual map for flight location** — overlay a bearing arc or mini-map showing the aircraft position relative to the configured home point on the TFT
- **Startup TFT display more informative of what is happening, not just "Searching"** — show OpenSky/AeroAPI step state, last fetch result and credit/quota indication
- **Touch input on CYD XPT2046** — currently unused; could drive card-next / pause / detail overlay
- **Per-airline accent colour** — FlightWall CDN dropped `brand_color_hex`; either embed a small static palette by ICAO or compute a dominant colour from the cached JPEG logo so cards stop being uniformly white

### Code optimisations (future revisions)
- [ ] `FlightWallFetcher::getAirlineData` — remove or repurpose the dead `brand_color_hex` parsing path; the CDN no longer returns the field
- [ ] `FlightDataFetcher::fetchFlights` — hoist `FlightWallFetcher fw` out of the per-flight loop and consider reusing a single `WiFiClientSecure` across airline/aircraft/logo calls to amortise TLS handshakes
- [ ] `FlightWallFetcher::getAirlineLogo` — simplify the duplicate-skip clause `(i == 1 && code == airlineIcao)`; an `if (codes[i] == codes[i-1]) continue;` is clearer
- [ ] Replace `client.setInsecure()` (AeroAPI + FlightWall + logo proxy) with pinned CA root certificates or fingerprint validation for production
- [ ] `AeroAPIFetcher` — call `time(nullptr)` once per request and reuse; current code reads it several times within `fetchFlightInfo`
- [ ] `AeroAPIFetcher` — consider migrating to ArduinoJson v7 streaming or a tighter custom parser to drop the 16 KB `DynamicJsonDocument` (largest single heap allocation in the request path)
- [ ] `WebUIServer::onGetLive` — currently allocates a 24 KB `DynamicJsonDocument` plus a `String` serialisation buffer; stream JSON directly to the response or use `WebServer::sendContent()` in chunks
- [ ] `WebUIServer::onGetLogo` — restrict the `name` argument to `[A-Z0-9]{2,4}\.jpg` rather than the looser current check (`indexOf('/')`, `..`, `.jpg` suffix); reduces attack surface even on LAN
- [ ] Add LRU / size cap to `/logos/` LittleFS cache so a long-lived device with diverse traffic does not exhaust the SPIFFS partition
- [ ] `parseIso8601` — fold the two `sscanf` calls into a single pass; minor but it runs per record per fetch cycle
- [ ] `RuntimeConfig::save` — write only changed keys to reduce NVS wear on configuration POSTs
- [ ] `OpenSkyFetcher` token expiry uses `millis()` — survives ~49-day wrap by arithmetic but triggers an unnecessary refresh at wrap; switch to `time(nullptr)` after NTP sync
- [ ] Watchdog — enable the task / interrupt watchdog and feed it from the main loop so a hung HTTP call resets rather than wedges
- [ ] `cyd_480x320` build flags carry a "verify TFT_BL pin and SPI_FREQUENCY against your exact board revision" comment from initial bring-up; verify on actual ST7796 hardware and remove the warning
- [ ] Touch calibration — `TFT_eSPI`/XPT2046 calibration matrix should be persisted in NVS (`Preferences`) before touch features land
- [ ] Encapsulate `g_flights` / `g_states` globals behind a small `AppState` class to make ownership and threading expectations explicit
- [ ] AeroAPI request budget (`MAX_AEROAPI_CALLS_PER_CYCLE = 5`) is compile-time; expose via `RuntimeConfig` and the WebUI for users on different paid tiers
- [ ] WebUI enriched-flights cap (5) is hardcoded in `WebUIServer::onGetLive`; lift to a constant alongside `EVENT_CAPACITY`
- [ ] `CYDDisplay::displayFlights` render-skip uses a string-concatenated `renderKey`; replace with a hashed key (e.g. FNV-1a over the same fields) to remove the `String` allocation per loop tick
- [ ] OpenSky `fetchStateVectors` — when WiFi reconnects mid-cycle, retry once instead of returning empty and forcing the next interval to refetch
- [ ] AeroAPI 401/403 are handled as generic non-200; treat 401 as a credential failure event surfaced in the WebUI Activity Feed
- [ ] `FlightDataFetcher::fetchFlights` clears `outStates` / `outFlights` unconditionally; consider `swap` with a temporary so callers' last-good vectors are never observed empty mid-fetch
- [ ] Reduce HTML/JS PROGMEM footprint by minifying `HTML_PAGE` at build time (current raw size ~10 KB)

---

## [1.0.1] 26-05-2026

### Fixed
- **Default coordinates replaced with Sydney CBD**: `UserConfiguration::CENTER_LAT/LON` changed from a precise six-decimal-place GPS fix to the Sydney CBD reference point (`-33.8688, 151.2093`). The previous value could identify a specific residential location. Runtime coordinates are always overridden via the WebUI and stored in NVS; this only affects the compile-time fallback used on first boot before any WebUI configuration.

---

## [1.0.0] 26-05-2026

### Initial full release

First stable release of the TheFlightWall CYD firmware. Consolidates the v0.12.0–v0.14.0 development cycle into a complete, documented build targeting the ESP32-2432S028R (ILI9341, 320×240) and ESP32-3248S035R (ST7796, 480×320).

### Summary of capabilities at v1.0.0

- **Enriched-only display policy**: only AeroAPI-enriched flights appear on the TFT and WebUI flight panel. Four categories suppressed: 1–2 letter ICAO prefix, pure-alpha callsigns, callsigns with no active AeroAPI match, and callsigns beyond the per-cycle call cap. All observed aircraft remain visible in the WebUI Activity Feed via raw OpenSky state vectors.
- **Full data pipeline**: OpenSky Network OAuth2 ADS-B → `FlightDataFetcher` callsign validation and ATC suffix stripping → AeroAPI `/flights/{ident}` enrichment → FlightWall CDN airline/aircraft names → LittleFS-cached JPEG logos → cycling TFT cards and embedded HTTP dashboard.
- **Web dashboard**: browser-rendered TFT mirror, horizontally scrollable current flights panel (all `g_flights` cards), 50-entry volatile activity feed, runtime configuration with NVS persistence and auto-reboot.
- **Live ADS-B metrics at no extra API cost**: distance, cardinal bearing, altitude/flight level, speed, heading, climb/descent rate and ground state from OpenSky, surfaced on both TFT and dashboard.
- **Documentation**: full README with pipeline diagram, display layout, credential setup guides, API references and image captions. CLAUDE.md architecture notes current.

---

## [0.14.0] 26-05-2026

### Added
- **Displayability filter** in `FlightDataFetcher`: ADS-B-only cards are now suppressed when the aircraft is on the ground without an active AeroAPI match, or when the callsign is absent (ident equals icao24). Enriched cards are always kept; airborne unenriched cards (e.g. AeroAPI rate-limited) are also kept. Resolves the empty "JST532 14.0km GROUND 0km/h" card type and the `7cf4b0` hex-only transponder card type appearing on the TFT and dashboard mirror.
- **Empty-state TFT message**: when aircraft are observed within radius but all are filtered out, the TFT shows `No active flights within Nkm` (where N is the runtime radius). The WebUI dashboard and TFT now agree on "nothing active".

### Fixed
- **`displayMessage` / `showLoading` flicker**: both methods now skip the SPI redraw when the requested text matches what is already on screen. Previously they redrew on every loop tick when the device was in an empty-state, causing visible flicker. `_lastStatusMessage` tracks the active text and is cleared when a flight card is drawn so a return to status text always redraws.
- **Stale "last-good" cards when only uninteresting traffic is in radius**: `main.cpp` now clears `g_flights` when the current fetch sees aircraft but none are displayable, so the TFT does not keep cycling a flight from minutes ago while the actual current reality is "only a parked aircraft is here".

### Changed
- `main.cpp` introduces `g_emptyMessage` and a three-way display dispatch at end-of-loop: flights → message → loading. `g_states` is retained on the filtered-empty path for WebUI activity-feed visibility.
- **WebUI activity feed lists every observed aircraft, not just displayable ones.** `WebUIServer::recordFetch` now iterates over raw OpenSky `states` rather than the post-filter `flights`, merging in enriched data by `icao24` when available. Each line is written in plain language: `JST532 — on ground, no active flight match  14km E · on ground`, `QFA1 — Sydney → Singapore (Qantas 737)  20km NW · FL360 · 920km/h climbing`, `7cf4b0 — unidentified ADS-B contact (Australia)  8km S · on ground`. Summary line now reports `Observed N nearby aircraft; M matched to a current flight; K shown on display.` Per-fetch detail capped at 12 lines (with `… and X more` suffix) to keep about four fetches of history visible inside the 50-entry ring buffer.
- **WebUI Current Flights panel**: removed the 5-flight enriched-only cap. The panel now shows every card in `g_flights` — the same set displayed on the TFT, including ADS-B-only airborne cards. Cards scroll horizontally with ‹ › navigation buttons and CSS scroll-snap. Enriched cards are tagged `ENRICHED` (green); ADS-B-only cards are tagged `ADS-B` (blue) and show airborne/ground status in place of the route line. The pill counter reads "N cards · M enriched". `GET /api/live` key renamed from `enriched` to `flights`; `DynamicJsonDocument` enlarged to 32 KB to accommodate larger flight lists.
- **WebUI title and footer**: page title updated to "The Flight Wall — CYD v0.14.0". Added footer with contact details (Anthony Clarke, email, GitHub/BlueSky/Threads/LinkedIn) and an acknowledgements grid crediting OpenSky Network, FlightAware AeroAPI, Jxck-S/airline-logos, images.weserv.nl, TFT_eSPI/Bodmer, TJpg_Decoder/Bodmer, WiFiManager/tzapu and ArduinoJson/Blanchon.
- **`MAX_AEROAPI_CALLS_PER_CYCLE` raised from 5 → 10**: near a major airport (e.g. 15 km of SYD) 7–10 in-range flights are common. The previous cap left the 6th+ flights permanently ADS-B-only. Each AeroAPI call takes ~10–15 s so the worst-case fetch cycle is ~150 s; the fetch interval is a minimum gap between cycles, so a slow fetch is safe.
- **ATC duplicate-departure suffix stripping**: callsigns like `QLK423D` where ATC appends a letter after the digits are now normalised to `QLK423` before the AeroAPI lookup. AeroAPI indexes by base flight number and returns no records for the suffixed form. The displayed ident on the TFT and dashboard remains the original broadcast callsign.
- **Enriched-only card policy**: only AeroAPI-enriched flights are shown on the TFT and the WebUI flight cards panel. Four categories are now suppressed rather than shown as "Airborne" placeholders: (1) 1–2 letter prefix callsigns (PE771 — non-standard ICAO); (2) pure-alpha callsigns with no flight number (CHK — helicopter/charter/government); (3) valid callsigns where AeroAPI was called and returned no active record (transition state — just rotated or on short final); (4) valid callsigns beyond the per-cycle AeroAPI call cap. All aircraft still appear in the WebUI Activity Feed via the raw OpenSky state vector path regardless of suppression. These flights can never be enriched by AeroAPI (ICAO airline codes are always 3 letters), so they would always render as `--- - ---` empty cards. 3-letter prefix flights still receive an AeroAPI enrichment attempt; hex-only idents are unchanged.
---

## [0.13.0] 25-05-2026

### Added
- **Live operations dashboard WebUI**: the embedded web interface now provides a low-overhead browser-rendered TFT mirror, an in-memory scrolling flight data feed, and detailed panels for up to five currently enriched flights. New `GET /api/live` and cached-logo `GET /api/logo` endpoints supply the live dashboard data.
- **Local diagnostic timestamps**: debug output is prefixed with local Australia/Sydney time after NTP synchronises, with boot elapsed-time prefixes available before synchronization.
- **Flight pipeline diagnostics**: runtime credential-presence, OpenSky query/filter counts and flight-card/enrichment totals are logged without disclosing secrets.

### Fixed
- **Live aircraft lost when enrichment was skipped**: accepted OpenSky state vectors now always produce ADS-B fallback cards, including invalid/unavailable callsigns and aircraft outside the per-cycle AeroAPI call allowance. The request limit now limits enrichment only rather than discarding displayable traffic.
- **Stale AeroAPI records selected for live aircraft**: matching now accepts only active, very recently arrived, or bounded no-arrival-time records. A historical record such as the CFH3 departure reported more than 70 hours earlier is rejected and leaves a live ADS-B-only card instead of showing a false route.
- **Timezone-dependent ISO conversion**: AeroAPI timestamps are converted to UTC without relying on the configured local timezone, allowing local debug timestamps without changing schedule calculations.
- **Credential exposure in WebUI**: `GET /api/config` returns configured/not-configured flags instead of stored OpenSky or AeroAPI credential values. Credential edits remain write-only with explicit clearing support.

### Changed
- Runtime configuration defaults expand the local search radius from 10 km to 15 km.
- `FlightInfo` now carries extended ADS-B identity, position, observation and transponder fields used by the live dashboard.
- OpenSky requests use explicit request timeouts and provide more useful live-query logging.
- The dashboard mirrors the TFT through browser rendering rather than framebuffer capture to avoid additional ESP32 SPI readback and image-transfer cost.

---

## [0.12.0] 25-05-2026

### Fixed
- **Timezone offset ignored in AeroAPI timestamps**: `parseIso8601` now reads and subtracts `+HH:MM` / `-HH:MM` timezone designators from AeroAPI ISO 8601 strings before converting to a UTC epoch. Previously the offset was silently stripped, causing timestamps from non-UTC airports (e.g. CZ325 Guangzhou→Sydney CST=UTC+8, SIA212 Sydney→Singapore AEST=UTC+10) to appear hours in the future. Effect: `buildDepartedLine` returned `""` and `buildArrivingLine` showed an arrival many hours wrong. Domestic flights on UTC-offset servers (e.g. QLK1427) were less affected.
- **Wrong AeroAPI flight record selected**: `AeroAPIFetcher` now scans all entries in the `flights[]` array and selects the one with the most recent departure epoch that is still in the past (≤ now), rather than blindly using `flights[0]`. AeroAPI returns flights sorted by `scheduled_out` descending, so `flights[0]` can be a future scheduled departure or a prior inbound leg rather than the currently-airborne flight. The fix resolves cases where the display showed a wrong origin/destination (e.g. "arriving in Sydney" for an outbound Sydney→Singapore flight) and wildly incorrect arrival countdowns.

### Changed
- `AeroAPIFetcher` logs the number of records returned, the selected index, and the raw departure/arrival timestamp strings with their parsed epochs and delta-from-now in minutes. Aids timezone and flight-selection diagnostics.

---

## [0.11.0] 25-05-2026

### Changed
- **Project restructured to standard PlatformIO layout.** All source files now live under `src/`: `src/adapters/`, `src/config/`, `src/core/`, `src/interfaces/`, `src/models/`, `src/utils/`. Only global utility headers (`debug.h`, `secrets.h`) remain in `include/`.
- All `#include` directives flattened — `"adapters/CYDDisplay.h"` → `"CYDDisplay.h"` etc. Each `src/` subdirectory has a corresponding `-I` entry in `[cyd_common]` build_flags.
- `build_src_filter` paths updated (no longer need `../` prefix; all `.cpp` are inside `src/`).
- `NeoMatrixDisplay.cpp/.h` and `config/WiFiConfiguration.h` (both unused legacy code) deleted.
- README and CLAUDE.md updated to reflect new paths.

---

## [0.10.0] 25-05-2026

### Changed
- Status lines now use city names instead of airport codes: "Departed Sydney 6 min ago", "Arriving at Melbourne in 45 min", "Arrived at Melbourne".
- `buildArrivingLine` now includes destination city and handles the arrived case with city name ("Arrived at Melbourne" rather than just "Arrived"). Hours+minutes format replaced the decimal-hours format for consistency with `buildDepartedLine`.
- `AirportInfo` model gains a `city` field populated from AeroAPI `origin.city` / `destination.city`.
- `AeroAPIFetcher` filter and parser extended to include `city` for both origin and destination.
- `CYDDisplay` adds `resolveCityOrCode()` helper — returns city name, falls back to IATA then ICAO code.

---

## [0.9.0] 25-05-2026

### Fixed
- **"Searching..." forever when AeroAPI fails**: `FlightDataFetcher` now always copies ADS-B state from the `StateVector` and pushes a `FlightInfo` to `outFlights` regardless of whether AeroAPI enrichment succeeds. Cards for unenriched flights show callsign, altitude, speed, heading, distance, and bearing; route and status lines are left empty (display falls back gracefully). Previously, any cycle where all AeroAPI calls failed resulted in an empty `outFlights`, causing the display to remain on the loading screen indefinitely.
- **Airline logo ICAO→IATA fallback**: `FlightWallFetcher::getAirlineLogo()` now accepts both `operator_icao` and `operator_iata`; if the ICAO download fails it retries with the IATA code. Resolves missing logos for carriers like Qantas (ICAO `QFA`, IATA `QF`) where the `flightaware_logos/` repository uses the two-letter IATA code as the filename.

### Changed
- `FlightWallFetcher::getAirlineLogo()` signature updated to `(airlineIcao, airlineIata, outLfsPath)`.
- `FlightDataFetcher` passes `info.operator_iata` alongside `info.operator_icao` to `getAirlineLogo()`.
- Callsign validation now also rejects callsigns with fewer than 3 leading alpha characters (`VV922` and similar non-ICAO prefixes that AeroAPI will never resolve).
- `AeroAPIFetcher` logs `content-len` and `doc-mem` alongside the "no flights in response" warning to distinguish empty API responses from filter/parsing failures.
- `README.md` display output section updated to describe the current commercial-style card layout.
- `README.md` adds full acknowledgements section crediting all libraries and data sources.

---

## [0.8.0] 25-05-2026

### Changed
- Airline logos now sourced from `github.com/Jxck-S/airline-logos` (`flightaware_logos/` directory, ~1 800 airlines by ICAO code) instead of FlightWall CDN.
- PNG files are fetched via `images.weserv.nl` which converts PNG→JPEG and resizes to `AIRLINE_LOGO_W × AIRLINE_LOGO_H` (80×80) in a single request. The JPEG result is cached in LittleFS as `/logos/{ICAO}.jpg` — no change to the TJpg_Decoder or LittleFS cache pipeline.
- `APIConfiguration` replaces `FLIGHTWALL_LOGO_CDN_PATH` with `AIRLINE_LOGO_PROXY_BASE`, `AIRLINE_LOGO_W`, `AIRLINE_LOGO_H`.
- `platformio.ini` adds `Bodmer/TJpg_Decoder` as an explicit `lib_dep` (no longer bundled in TFT_eSPI 2.5.x); include changed from `<TJpgDec.h>` to `<TJpg_Decoder.h>`.
- `platformio.ini` adds `-I` paths for `FS/src` and `LittleFS/src` (Arduino ESP32 3.x requires explicit framework library paths).

---

## [0.7.0] 25-05-2026

### Fixed
- **AeroAPI NoMemory** (`UAE1DP`, `JAL51`, `JST501`, `AM211`): `DynamicJsonDocument` raised from 4 096 to 16 384 bytes. ArduinoJson's filter retains ALL matching array elements, not just `flights[0]`; large responses with 15+ historical flights exceeded the old limit. Also switched from `http.getString()` to `http.getStream()` so the raw 20–30 KB body and the doc are never both in heap simultaneously.
- **HTTP 400 / junk callsigns** (`RED O`, `DELTA`): `FlightDataFetcher` now trims trailing whitespace (OpenSky pads callsigns to 8 chars) then rejects callsigns with no digit or an embedded space before calling AeroAPI.
- **HTTP 429 rate limit**: `FlightDataFetcher` now stops after `TimingConfiguration::MAX_AEROAPI_CALLS_PER_CYCLE` (default 5) valid callsign attempts per fetch cycle. State vectors are already distance-ordered so the 5 closest aircraft are always preferred.

### Changed
- `TimingConfiguration` adds `MAX_AEROAPI_CALLS_PER_CYCLE = 5`.
- `AeroAPIFetcher` logs `payload_len` from `http.getSize()` (Content-Length header) rather than the full response string length.

---

## [0.6.0] 25-05-2026

### Added
- Airline logo images: JPEG logos downloaded from FlightWall CDN on first encounter and cached to LittleFS (`/logos/{ICAO}.jpg`); rendered via TJpgDec (bundled in TFT_eSPI). Colored airline text remains as fallback when no logo is cached.
- LittleFS initialised at startup (`LittleFS.begin(true)` formats on first boot); `/logos/` directory created automatically.
- "Departed {IATA} X min ago" format: origin airport code now included in the departed status line.

### Changed
- `buildDepartedLine` prefixes origin airport IATA/ICAO code; time format switches from float to integer hours + minutes remainder (e.g. "2h 15m ago").
- `FlightWallFetcher` adds `getAirlineLogo()` (checks LittleFS cache, downloads if missing) and `httpGetToFile()` (streams HTTP response directly to a `File` via `writeToStream`).
- `FlightInfo` model gains `logo_path` (LittleFS path to cached logo JPEG).
- `FlightDataFetcher` calls `getAirlineLogo()` after `getAirlineData()` for each enriched flight.
- `CYDDisplay::drawFlightCard()` split into two phases: Phase 1 draws all text elements inside `startWrite()`/`endWrite()`; Phase 2 renders the JPEG logo after `endWrite()` to avoid SPI transaction nesting.
- `CYDDisplay::initialize()` sets up TJpgDec: scale 1, swap bytes enabled, `jpegOutputCb` callback.

---

## [0.5.0] 25-05-2026

### Added
- Commercial-style flight card layout matching the FlightWall product display
- Airline logo: name rendered in the airline's brand color (fetched from FlightWall CDN `brand_color_hex` field); white fallback if CDN has no color entry
- IATA airport codes (`LAX`, `JFK`) preferred over ICAO for route display; ICAO fallback retained
- "Departed X min ago" / "Arriving in Y hr" status lines derived from AeroAPI `actual_out` / `estimated_in` timestamps
- Flight progress bar at bottom of card: green fill proportional to elapsed flight time
- NTP time sync via `configTime(0, 0, "pool.ntp.org")` called after WiFi connects; status lines and progress bar are hidden until sync succeeds
- Live ADS-B metrics (altitude, speed, heading) shown as fallback when no AeroAPI schedule data is available

### Changed
- `FlightWallFetcher::getAirlineName()` replaced by `getAirlineData()` — fetches display name and brand color in a single HTTP request
- `AeroAPIFetcher` filter document enlarged to 768 bytes; now parses `origin.code_iata`, `destination.code_iata`, `actual_out`, `estimated_in`, `scheduled_out`, `scheduled_in`
- `AirportInfo` model gains `code_iata` field
- `FlightInfo` model gains `airline_color` (uint16_t RGB565), `actual_out_epoch`, `estimated_in_epoch` (time_t)
- `CYDDisplay` render key includes per-minute time bucket so status lines and progress bar refresh every minute
- `UserConfiguration` gains `COLOR_CALLSIGN`, `COLOR_PROGRESS`, `COLOR_PROGRESS_BG`; `COLOR_DIVIDER` darkened

---

## [0.4.0] 24-05-2026

### Added
- `RuntimeConfig` module — NVS-backed runtime configuration (Preferences, namespace `flightwall`); replaces all compile-time constants for location, timing, brightness, and API credentials
- `WebUIServer` — HTTP server on port 80 serving a single-page configuration UI; routes `GET /`, `GET /api/config`, `POST /api/config`
- WebUI HTML page embedded as PROGMEM in `WebUIServer.cpp`; dark amber theme, no external dependencies
- Configurable fields: latitude, longitude, radius, OpenSky client ID/secret, AeroAPI key, fetch interval, card cycle duration, backlight brightness
- Save & Reboot flow: POST handler persists to NVS, sets a flag; `main.cpp` reboots via millis-based delay to allow TCP flush

### Changed
- `FlightDataFetcher` now reads location from `RuntimeConfig` instead of `UserConfiguration`
- `CYDDisplay` now reads display cycle duration from `RuntimeConfig` instead of `TimingConfiguration`
- `OpenSkyFetcher` now reads OAuth credentials from `RuntimeConfig` instead of `APIConfiguration`
- `AeroAPIFetcher` now reads AeroAPI key from `RuntimeConfig` instead of `APIConfiguration`
- `main.cpp` reads fetch interval from `RuntimeConfig`; calls `RuntimeConfig::load()` before any other init
- `platformio.ini` adds `WebServer/src` explicit include path and new `.cpp` files to `build_src_filter`

---

## [0.3.0] 24-05-2026

### Added
- Live OpenSky metrics on CYD flight cards: distance, bearing, altitude/flight level, speed, heading, climb/descent, and ground state
- Cached flight-list display cycling independent of the network fetch interval
- Root `include/secrets.h` ignore rule, plus generated `.pio` and `.vscode` ignores
- Expanded README documentation for OpenSky OAuth2, AeroAPI authorisation, FlightWall CDN lookups, rate limits, and current display behavior

### Changed
- Moved the active PlatformIO project layout from `firmware/` to the repository root
- `FlightInfo` now carries live ADS-B state fields copied from matching OpenSky state vectors
- `CYDDisplay` now avoids unnecessary redraws when the selected card has not changed
- `AeroAPIFetcher` now uses filtered ArduinoJson parsing to reduce heap use and avoid `NoMemory` on large AeroAPI responses
- `AeroAPIFetcher` now logs readable HTTPClient errors and uses a longer request timeout
- `main.cpp` keeps the last good enriched flight list when later fetches are empty or rate-limited
- PlatformIO config includes explicit Arduino ESP32 3.x core include paths required by `Network`, `HTTPClient`, and `NetworkClientSecure`

### Fixed
- Arduino ESP32 3.x compile failure for missing `Network.h`
- Missing `HTTPClient.h` / `WiFiClientSecure.h` include discovery and link errors
- TFT_eSPI GFX font redefinition errors
- Arduino ESP32 3.x LEDC API compatibility for TFT backlight PWM
- Display appearing stuck on a single flight because display cycling was gated by network fetch completion
- `DBG_INFO` macro usage that failed when passed a conditional string expression directly

---

## [0.2.0] 24-05-2026

### Added
- `CYDDisplay` adapter — renders flight cards on ESP32 CYD via TFT_eSPI (ILI9341 and ST7796)
- Two PlatformIO environments: `cyd_320x240` (ESP32-2432S028R) and `cyd_480x320` (ESP32-3248S035R)
- `include/debug.h` — leveled debug macros (`DBG_ERROR` / `DBG_WARN` / `DBG_INFO` / `DBG_VERBOSE`)
- `include/secrets.h.template` — credential template (copy to `secrets.h`, never commit)
- `partitions_custom.csv` — custom partition table for 4MB flash
- `CHANGELOG.md`

### Changed
- `BaseDisplay` interface extended with `displayMessage()` and `showLoading()`
- `APIConfiguration.h` now loads credentials from `include/secrets.h` via `__has_include`
- `HardwareConfiguration.h` replaced LED matrix config with TFT backlight PWM constants
- `UserConfiguration.h` colours updated to RGB565 palette; location example set to Sydney AU
- `main.cpp` refactored: WiFiManager provisioning, `debug.h` macros, no `delay()` in `loop()`
- ArduinoJson pinned to `^6.21.0` — fixes v7/v6 API mismatch in existing fetcher code

### Removed
- FastLED / Adafruit GFX / FastLED NeoMatrix dependencies (LED matrix hardware target)
- Hardcoded WiFi credentials (`WiFiConfiguration.h` superseded by WiFiManager)

---

## [0.1.0] — Initial release

### Added
- OpenSky OAuth state vector fetching with geo filter
- AeroAPI flight enrichment
- FlightWall CDN airline/aircraft name lookup
- NeoMatrixDisplay — WS2812B LED matrix renderer (legacy, retained in `adapters/`)
- Core data pipeline: `FlightDataFetcher` orchestrating fetch → enrich → display
