# Changelog

Outstanding features and code optimisations are tracked separately in [TODO.md](TODO.md).

---

## [Unreleased]

---

## [1.3.0] 30-05-2026

### Added
- **Splash screen at boot** — `CYDDisplay::showSplash()` decodes `/splash.jpg` from LittleFS via TJpgDec at startup. The correct variant (`splash_320x240.jpg` / `splash_480x320.jpg`) is copied into `data/splash.jpg` by `scripts/copy_splash.py`, a PlatformIO pre-build script that selects the file based on `PIOENV`. Falls back to `displayMessage("FlightWall")` text banner if the image is missing. `showSplash()` is idempotent: sets `_splashOnScreen = true` on the first decode; subsequent calls while the state is unchanged are no-ops. Any fullscreen draw (flight card, map card, `displayMessage`, `showLoading`) resets the flag so a return to no-data state always re-decodes.
- **Splash-as-background in all no-data states** — the splash image stays visible throughout startup and all empty states. WiFi connecting, NTP sync, "No active flights within Nkm" and "Searching…" all display their messages in the bottom status bar, leaving the splash untouched. `g_hasEverShownFlights` flag removed; loop tail simplified to two branches: flights → flight cards; otherwise → `showSplash()` + `showFetchStatus()`.
- **`CYDDisplay::showFetchStatus()`** — 24 px amber-on-navy status bar pinned to the very bottom of the TFT. Idempotent on identical phase text; used for all startup messages (`"Connecting WiFi…"`, `"WiFi OK: x.x.x.x"`, `"Searching…"`) and mid-fetch phase names (`"OpenSky"`, `"AeroAPI 3/10"`, `"Map cache"` etc.). Replaces fullscreen `showLoading()` / `displayMessage()` in all status paths so the splash always remains behind the message.
- **`src/config/Version.h`** — single source of truth for `FW_VERSION_STR`. The WebUI HTML title and brand badge embed the macro via adjacent raw-string-literal concatenation (`R"rawlit(... v)rawlit" FW_VERSION_STR R"rawlit(</title>)rawlit"`) — zero runtime cost, no string copy. The boot serial banner logs the same value. Markdown docs (`README.md`, `CLAUDE.md`, `CHANGELOG.md`) remain manually maintained; the header comment lists all locations that must be touched on a version bump.
- **GitHub Actions release workflow** (`.github/workflows/release.yml`) — fires automatically when a clean `vX.Y.Z` tag is pushed to main (pre-release suffixes `-dev` / `-rc` are excluded by the tag glob). Builds both `cyd_320x240` and `cyd_480x320` firmware targets via PlatformIO on Ubuntu; extracts the matching `## [X.Y.Z]` block from `CHANGELOG.md` as release notes; creates a GitHub Release with both `.bin` files attached. The shields.io release badge on `README.md` updates automatically from the GitHub Releases API. No manual `gh release create` step required.
- **WebUI brand refresh** — Space Grotesk (headings) and JetBrains Mono (data/code) loaded from Google Fonts; full CSS brand-token sweep replacing ad-hoc hex values with `--amber:#ff9b2e`, `--ink:#f3f4f6`, `--ink-dim:#8a8f99`, `--cyan:#5fb7d6`, `--green:#3ddc7a`; inline SVG logo in the dashboard header; base64 favicon data URI in `<head>` (no separate HTTP request). Branding assets live in `assets/branding/` and are gitignored from `data/splash.jpg` (generated per-build).
- **`README.md` professional header** — `<picture>` element with theme-aware logo variants (dark/light system preference); shields.io badge row: release, license, last-commit, platform (ESP32), PlatformIO, Arduino framework.

### Changed
- Boot sequence: all status messages during WiFi connect and NTP sync now call `showFetchStatus()` instead of `displayMessage()` / `showLoading()`, so the splash screen stays on screen throughout the entire startup flow.
- `initWiFi()` displays `"Connecting WiFi…"`, `"WiFi OK: x.x.x.x"` (or `"No WiFi"`) in the bottom bar; `setup()` tail leaves `"Searching…"` in the bar. No fullscreen status clears the splash.
- `UserConfiguration::COLOR_MAP_LABEL` default corrected to dark blue (`#00008B`, RGB565 `0x0011`).

### Documentation
- `CLAUDE.md` updated: `Version.h` as canonical version source; slot-tracking parity architectural note (WebUI `onGetLive()` must mirror `displayFlights()` `totalSlots` exactly); countdown timer source-of-truth; release process sequence.
- `README.md` current release line updated; `src/config/` entry in Key Components table now lists `Version.h`.

---

## [1.2.0] 29-05-2026

### Added
- **WebUI fetch-busy indicator** — a pulsing amber banner appears across the full dashboard width whenever a fetch cycle is in progress. Phase text updates as the device walks the loop (`"OpenSky"`, `"AeroAPI 3/10"`, `"Airline logo"`, etc.). If the 1 s poll aborts mid-HTTPS-call the banner shows `"Device busy: Awaiting response…"` until the response returns. Implemented via `FetchProgressCb` callback in `FlightDataFetcher`, `WebUIServer::pump()` / `setBusy()` called between sub-calls, `busy` + `phase` fields on `GET /api/live`, and an `AbortController` (1.5 s timeout) in the JS `poll()` function replacing the previous bare `fetch()`.
- **CYD fetch status bar** — while a fetch is in progress the CYD TFT shows a thin amber-on-navy status bar at the very bottom of the screen, displaying the current phase (`"Fetching…"`, `"OpenSky"`, `"AeroAPI 3/10"`, `"Airline logo"`, `"Map cache"`, etc.). The bar is cleared and the render guard invalidated when the fetch completes so the next `displayFlights()` call redraws cleanly. Implemented in `CYDDisplay::showFetchStatus()`; driven by the same `FetchProgressCb` callback as the WebUI busy indicator via a `FetchCtx` struct capturing both `WebUIServer*` and `CYDDisplay*`.
- **`GET /api/screenshot` — pixel-perfect TFT framebuffer capture as BMP** — new route streams a 24-bit Windows BMP of the live CYD screen for documentation, layout verification and visual debugging. Uses `TFT_eSPI::readRectRGB()` (via `CYDDisplay::readRectRGB()` pass-through, kept narrow to avoid leaking `_tft`) which returns clean RGB888 without the byte-swap that `readRect()` applies for `pushRect()` compatibility. Streams row-by-row with `WebServer::setContentLength()` + `sendContent()`; one 1 440-byte stack buffer covers both 320×240 and 480×320 targets (zero heap). BMP `biHeight` is negative so rows are stored top-to-bottom, matching natural scan order. Filename is timestamped (`cyd_YYYYMMDD_HHMMSS.bmp` when NTP is synced, `cyd_t{millis}.bmp` fallback). Capture takes ~150–200 ms of SPI reads + ~1–2 s of TCP transfer; safe because `WebServer::handleClient()` blocks the loop synchronously so no concurrent display writes can occur. WebUI exposes a "⬇ Screenshot (BMP)" link in the TFT Mirror panel footer. Enabled by two new build flags in `platformio.ini`: **`-DTFT_MISO=12`** (the CYD's ILI9341 SDO pin — not previously declared, hence `readRectRGB` returned `0xFF` floating bus and all-white BMPs) and **`-DSPI_READ_FREQUENCY=6250000`** (reduced from 20 MHz; the CYD's MISO trace has no termination resistors and 20 MHz reads were unreliable). Verified working on `cyd_320x240` (ESP32-2432S028R, ILI9341). Known risk: GPIO 12 is an ESP32 strapping pin (MTDI) — should boot fine because the ILI9341 tri-states MISO when CS is high, but a strong pull-up on any specific board could cause boot failures; revert `-DTFT_MISO=12` if the device fails to boot.
- **WebUI map-card mirror** — when the CYD enters its map-card slot the WebUI TFT Mirror panel now swaps its flight card for the actual map: the cached `/api/mappreview` JPEG with the same SVG flight-marker overlay used in the Device Configuration preview panel. An amber **● MAP CARD** pill appears in the panel header and the counter changes to `MAP`. Required a server-side fix: `onGetLive()` previously did `currentFlightIndex() % flights.size()` which collapsed the map slot back to flight #0, making it invisible to the dashboard; corrected to mirror `CYDDisplay::displayFlights()` exactly (`totalSlots = flightCount + 1`, map slot when `slotIdx == flightCount`), with a new `screen.kind = "flight" | "map"` field on `/api/live`. The `updateMapOverlay()` JS function was generalised to accept arbitrary `(img, svg)` targets so the mirror and the Device Config preview share one renderer.
- **WebUI "Next update" countdown** — the dashboard header now shows `Device live HH:MM:SS | Next update XXs | N API credits`, with the countdown driven by a new `next_fetch_in` field on `/api/live`. Computed server-side from `millis()` arithmetic (`_lastFetchMs` tracked alongside `_lastFetchEpoch` in `recordFetch`) so the countdown is immune to browser / device clock skew or NTP drift. Three display states: `First fetch pending` during the 8 s startup grace; `Fetch in progress` while busy (takes precedence over the timer so it doesn't read `0s` next to the busy banner); otherwise `Next update Ns`.
- **Configurable map label colour** — flight marker colour (dot, heading tick, callsign label) for enriched flights on both the CYD map card and the WebUI map preview is now user-configurable. `UserConfiguration::COLOR_MAP_LABEL` (dark blue `#00008B`, RGB565 `0x0011`) is the compile-time default. Persisted to NVS key `lbl_col` via `RuntimeConfig::labelColor()` / `setLabelColor()`. Exposed as a colour picker in the **Device Configuration** panel alongside the other display settings; changes take effect on the CYD after Save & Reboot. The WebUI map preview updates live as the picker is dragged.
- **Device Configuration colour picker with tooltip** — the map label colour field includes a CSS `::after` tooltip (ⓘ amber icon, hover to reveal) explaining that the setting applies to both the CYD and the WebUI preview and is saved to NVS. Implemented as a CSS-only tooltip using `data-tip` + `content: attr(data-tip)` to avoid unreliable native `title` tooltip behaviour on macOS/Chrome.

### Changed
- **TFT resolution displayed in panel title** — the WebUI "TFT Mirror" panel title now reads `TFT Mirror — 320 × 240 physical` (or `480 × 320` for the ST7796 variant). Resolution is set dynamically from the `screen.width` / `screen.height` fields in each `/api/live` poll response; the old static `id="resolution"` span in the panel footer is removed.
- **Removed "Preview Map" button** — the `POST /api/showmap` route, `onShowMap()` handler, `shouldShowMap()` / `clearShowMap()` flag, and the corresponding `main.cpp` hold-timer block are all removed. The "Fetch Map" button in Device Configuration subsumes the testing use-case; the map card appears in the normal display cycle.
- **Removed "Last scan" timestamp** — the `id="cardtime"` span below the TFT mirror has been removed along with its `d.last_fetch` assignment in `poll()`.
- **"MAP" label removed from map card header** — the top-left `"MAP"` text on the CYD radar map card is removed; the header strip now shows only the flight count (e.g. `"3 flights"`) in the top-right, keeping the map area clean.
- `UserConfiguration::COLOR_MAP_LABEL` default is dark blue (`#00008B`, RGB565 `0x0011`); added alongside `COLOR_MAP_UNENR` — label colour is now a named constant rather than hardcoded in `RuntimeConfig`.
- `WebUIServer` POST `/api/config` and GET `/api/config` both include `label_color` as an `#rrggbb` hex string. RGB565 ↔ hex conversion performed in the handler (no new dependencies).

### Fixed
- **WebUI map overlay missing flights** — previous session's `on_ground` filter in `updateMapOverlay()` removed; JS now matches `CYDDisplay::drawMapCard()` behaviour (enriched ground aircraft are included in both).
- **CSS tooltip not shown on hover** — native `title` attribute replaced with `data-tip` + CSS `::after` pseudo-element for the ⓘ info icon; renders immediately and reliably on all browsers.
- **WebUI callsign mismatch with CYD** — the dashboard showed `VOZ804` (ICAO broadcast) while the CYD showed `VA804` (IATA, more commonly recognised). The server-side `addFlight()` serialiser now resolves `ident` using the same precedence as `CYDDisplay::resolveCallsign()`: `ident_iata` → `ident` → `ident_icao`. The raw `ident_icao` / `ident_iata` JSON fields are retained for callers that need them. Fixes both the TFT Mirror callsign and the Current Flights detail cards in one change.
- **CYD map slot invisible to the WebUI** — `WebUIServer::onGetLive()` previously applied `currentFlightIndex() % flights.size()`, so the map slot (`index == flights.size()`) collapsed back to flight #0 and the dashboard never reflected the map-card state of the TFT. Now uses `totalSlots = flightCount + 1` to match the CYD cycle exactly and emits `screen.kind` to disambiguate the two slot types.
- **Fetch status bar text clipped at screen edge** — `CARD_BAR_H + 2` was too thin to vertically centre `F_SUB` glyphs; bumped to `CARD_BAR_H * 2` (24 px on 320×240, 36 px on 480×320) so descenders no longer overflow the bottom of the TFT.

### Documentation
- **`images/pipeline.png` regenerated** — new diagram shows all five external sources (OpenSky, AeroAPI, FlightWall CDN, Jxck-S/airline-logos, Google Maps Static API), the `images.weserv.nl` baseline-JPEG proxy, the on-device orchestrator (FlightDataFetcher + MapProvider + LittleFS + NVS), and both outputs (CYDDisplay + WebUIServer).
- **`README-API.md`** — new *How the pipeline works* section added above the services table, describing the 6-step per-cycle data flow with the role of each service and the two LittleFS-cached outputs.
- **`README.md` — Appendix A: Hardware background** — new appendix for readers unfamiliar with the CYD family. Covers what the device is (with credit to Brian Lough for the name), a board-variants comparison table covering seven Sunton SKUs, where to buy, community 3D-printable cases (including [thingiverse.com/thing:6440252](https://www.thingiverse.com/thing:6440252)), community resources (Brian Lough's [ESP32-Cheap-Yellow-Display repo](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) and [Discord channel](https://discord.com/channels/630078152038809611/1109228361441620028), Random Nerd Tutorials, Volos Projects), and known hardware quirks (MISO/GPIO 12 wiring, strapping-pin behaviour, SPI bus sharing). Linked from the top of the README.
- **`TODO.md` split out of `CHANGELOG.md`** — the Todo section had grown to roughly half the CHANGELOG by line count; moved to its own file with a short pointer line in `CHANGELOG.md`. No content lost.
- **`CHANGELOG.md` code optimisations** — three already-completed items removed during v1.2.0; remaining items updated with correct doc sizes and config values; new items added covering the FreeRTOS-task migration, the watchdog dependency, the `showStandaloneMap` dead-code cleanup, and HTML/JS footprint growth from this release.

---

## [1.1.0] 27-05-2026

### Added
- **Google Maps card in display cycle** — a Google Maps Static API image (road-map style, JPEG) is fetched once and cached in LittleFS for 24 hours. It appears as the final slot in the flight-card cycle, showing all tracked flights overlaid as coloured dots with heading ticks and truncated callsigns. Amber = enriched (AeroAPI route matched); cyan = ADS-B-only. Cache auto-invalidates when the configured centre or radius changes.
- **`map_sec` runtime config** — controls how long the map card stays on screen. Defaults to 15 s (vs 3 s for flight cards). Exposed in the WebUI Device Configuration panel and persisted to NVS (`map_sec` key). `cycle_sec` continues to govern individual flight cards independently.
- **`MapProvider` adapter** (`src/adapters/MapProvider.cpp`) — encapsulates all map-tile logic: URL construction, HTTPS fetch via `writeToStream()` to LittleFS (no full-body heap allocation), NVS cache-metadata management (`mapcache` namespace), Web Mercator lat/lon → pixel projection, and zoom auto-selection from `radius_km`.
- **`SECRET_MAPS_API_KEY`** placeholder added to `include/secrets.h.template`. Enable _Maps Static API_ in Google Cloud Console and add the key to your local `secrets.h`.
- **WebUI startup grace window** — `TimingConfiguration::STARTUP_WEBUI_GRACE_MS` (default 8 s) delays the first blocking API fetch so the WebUI is reachable immediately after WiFi connects. Useful for debug — connect to `http://<ip>/` before any API calls begin. Logged as `WebUI ready — first fetch in 8 s`.
- **OpenSky 429 backoff** — on a 429 response the firmware reads the `X-Rate-Limit-Retry-After-Seconds` header and suspends all OpenSky fetch attempts for that duration (defaults to 1 hour if header is absent). Subsequent attempts during the window are skipped immediately with a countdown log and no network call.
- **WebUI "Preview Map" button** — a secondary button in the TFT Mirror panel footer sends `POST /api/showmap`; `main.cpp` holds the CYD on the map card for `mapDisplaySec()` seconds then returns to normal cycling. Useful for testing the map card while rate-limited or when no flights are present. The `CYDDisplay::showStandaloneMap()` method renders the map with zero flight markers if `g_flights` is empty.
- **WebUI "Fetch Map" button** — a secondary button in the Device Configuration panel reads the current lat/lon/radius form values and sends `POST /api/fetchmap`. The handler updates `RuntimeConfig` in memory (no NVS save, no reboot), sets the show-map flag, and the existing map-preview pipeline in `main.cpp` calls `MapProvider::ensureMapCached()` which detects the changed coordinates and downloads a fresh map tile. Allows validating that a location and radius show the expected area before committing with "Save & Reboot".
- **OpenSky credits remaining** — `updateCreditsFromHeader()` reads `X-Rate-Limit-Remaining` after every successful 200 on both the main fetch path and the 401-retry path, and now also on 429 responses (OpenSky includes the header there too, typically showing 0 when daily credits are exhausted — this surfaces the count in the WebUI immediately rather than waiting for the backoff to lift). Logs `[INFO] OpenSky: credits remaining today: N` (≥300) or `[WARN] OpenSky: credits LOW — N remaining today` (<300). The first time credits drop below 300 a one-time warning is appended to the WebUI activity feed; the flag resets once credits recover above 500 (e.g. after midnight). In the WebUI header status bar the count appears in muted grey (≥500), amber (<500), or red (<200); the api-alert banner also fires at <200 with the count and advice to raise the fetch interval or wait for the daily reset.
- **API error surface** — OpenSky failures (rate limit, auth, network) now appear on the CYD TFT in place of "Searching…" and as a red alert bar immediately below the WebUI dashboard header. Both clear automatically on the next successful fetch.
- **`README-API.md`** — new standalone API reference covering all five services (OpenSky, AeroAPI, Google Maps Static, FlightWall CDN, images.weserv.nl) with credential setup, pricing, runtime behaviour, hints and diagnostic message tables.

### Changed
- `TimingConfiguration::DISPLAY_MAP_SECONDS` (15) added alongside `DISPLAY_CYCLE_SECONDS` (3).
- `RuntimeConfig`: `mapDisplaySec()` / `setMapDisplaySec()` added; `save()` / `load()` extended with NVS key `map_sec`.
- `CYDDisplay::displayFlights()` now uses `mapDisplaySec()` while the map slot is active and `displayCycleSec()` for flight cards — the two timers are independent.
- `CYDDisplay::mapRenderKey()` now hashes flight lat/lon (0.01° resolution) and the `MapProvider::mapVersion()` counter instead of bearing/distance.
- `WebUIServer` `/api/config` GET and POST both include `map_sec`.
- **OpenSky `http.begin()` fixed for ESP32-Arduino 3.x** — all three HTTPS call sites in `OpenSkyFetcher` (token POST, state vector GET, 401 retry) now pass an explicit `WiFiClientSecure` with `setInsecure()`. Bare `http.begin(url)` on 3.x does not call `setInsecure()` internally, causing TLS failures reported as `-1`.
- **Token log messages humanised** — `expires_in: 1800s` → `valid for 30:00`; `expires at ms: NNNNN` → `valid until HH:MM:SS` (local Australia/Sydney time, post-NTP).
- **Rate-limit log messages humanised** — seconds converted to `Xh Ym` / `Xm Ys` via `fmtDuration()`; initial 429 log now includes `(clears at HH:MM)` wall-clock time.
- **HTTP failure diagnostics** — OpenSky and MapProvider error log lines now include `wifi: N` (WiFi status code) and `heap: N` bytes, allowing WiFi loss to be distinguished from TLS or server errors.
- **`README.md`** — API detail replaced with a lean summary table and essential setup steps for each credentialed service; full detail moved to `README-API.md`. Map card added to Current status, What it does, Key components, Display output and Notes sections. "What this edition adds" block added to the project header.

### Fixed
- **Map card blank — progressive JPEG** — TJpg_Decoder returns `JDR_FMT3` ("not supported JPEG standard") because Google Maps Static API returns progressive JPEG. `MapProvider` now routes the request through `images.weserv.nl` (already used for airline logos) which re-encodes the output as baseline JPEG. A cache format-version key (`fmtv=1`) was added to the NVS `mapcache` metadata so any existing cached progressive JPEG is automatically detected as stale and re-fetched. `drawMapCard()` continues to use `TJpgDec.drawJpg()` (memory decode) rather than `drawFsJpg()` for reliability.
- **WebUI map preview** — a `GET /api/mappreview` endpoint streams the cached `/mapcache.jpg` directly from LittleFS to the browser (the browser handles JPEG natively regardless of encoding type). A map preview image with a "↺ Refresh" button now appears in the Device Configuration panel between the location/radius fields and the credentials section. It loads automatically on page open (hidden if no cache), and auto-refreshes ~10 s after a successful "Fetch Map" request.
- **Rapid-fire 429 retry loop** — `g_flights.empty()` removed from the `shouldFetch` condition. When any fetch returned zero flights the condition was `true` on every subsequent `loop()` tick, firing OpenSky calls every ~1 s and exhausting the daily credit quota in minutes. The first fetch now fires at grace-expiry via a pre-armed `g_lastFetchMs` and subsequent fetches respect the configured interval regardless of flight list state.
- **"Searching…" shown on API failure** — `g_emptyMessage` is now set to the `OpenSkyFetcher::lastError()` string when a fetch fails with no last-good data. Previously the empty-result code path set `g_emptyMessage = ""`, causing the TFT to display "Searching…" indefinitely during rate-limiting or auth failures.

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
