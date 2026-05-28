# TODO — TheFlightWall CYD Edition

Outstanding work split out of `CHANGELOG.md` for readability. The changelog records what shipped; this file tracks what hasn't. Items are grouped into **Features** (visible user-facing changes) and **Code optimisations** (internal refactors, hardening and cleanups).

When an item ships, it should be moved to the relevant `## [x.y.z]` section in `CHANGELOG.md` and removed from here.

---

## Features

- ~~**Visual map for flight location** — overlay a bearing arc or mini-map showing the aircraft position relative to the configured home point on the TFT~~ — **done in v1.1.0**
- ~~**Startup TFT display more informative of what is happening, not just "Searching"** — show OpenSky/AeroAPI step state~~ — **step state done in v1.2.0 (fetch status bar)**; last fetch result and credit/quota on TFT remain outstanding
- **Pinned flight tracking** — allow the user to nominate a specific flight number (e.g. `QF1`, `VA432`) in the WebUI Device Configuration. That flight is tracked every fetch cycle regardless of whether it falls within the configured radar radius, and its card is always pinned as the first item in the CYD display cycle.

  **Data sources by scenario:**

  | Scenario | Position data | Route / schedule data |
  |:---------|:-------------|:----------------------|
  | Within radar radius | OpenSky bounding-box query (existing path) | AeroAPI enrichment (existing path) |
  | Outside radar radius | Targeted OpenSky query: `GET /api/states/all?callsign={callsign}` — returns the state vector regardless of position; costs one standard credit | AeroAPI `/flights/{ident}` (existing path, same cost as in-range) |
  | On ground / not yet departed | No position or AeroAPI match; show schedule-only card with departure countdown derived from AeroAPI `scheduled_out` | AeroAPI may still return the next scheduled leg |
  | Completed / arrived | No live state vector; AeroAPI returns historical record | Show "Arrived at {city}" card with elapsed time since arrival |

  **Callsign matching:** OpenSky broadcasts 3-letter ICAO callsigns padded to 8 chars (e.g. `QFA001  `). The `callsign` query parameter accepts the trimmed form. The existing ATC-suffix stripping in `FlightDataFetcher` already normalises forms like `QLK423D → QLK423`. IATA-to-ICAO callsign mapping (e.g. user enters `QF1`, device queries `QFA001`) is the main friction point — options: (a) store both ICAO and IATA form in config and try both, (b) derive ICAO from the AeroAPI `operator_icao` + flight-number fields on first successful enrichment and cache to NVS, (c) document that the user must enter the ICAO broadcast form.

  **`RuntimeConfig` / NVS additions:**
  - `track_cs` (String, max 8 chars) — stored callsign, empty string means no pinned flight
  - Exposed in WebUI Device Configuration as a text field alongside the other settings
  - Set via `POST /api/config` alongside existing fields; no reboot required (picked up on next fetch)

  **`FlightDataFetcher` additions:**
  - After the main bounding-box fetch loop completes, check `RuntimeConfig::trackedCallsign()` — if non-empty and not already present in `outFlights` (in-range path already handled it), run the targeted fetch:
    1. `GET /api/states/all?callsign={callsign}` — if a state vector is returned, construct a `StateVector`, compute `distance_km` and `bearing_deg` from home via `GeoUtils`, then run the standard AeroAPI enrichment path.
    2. If OpenSky returns no state vector (flight not yet airborne, between legs, or ADS-B shadow), fall through to AeroAPI-only: call `AeroAPIFetcher` directly for schedule data and construct a position-less `FlightInfo` — `distance_km` and `bearing_deg` will be `NaN`, position fields empty.
  - Insert the tracked `FlightInfo` at index 0 of `outFlights` regardless of distance sorting.
  - Mark it with a new `FlightInfo::pinned = true` bool so display code can badge it distinctly.

  **CYD display card for pinned flight:**
  - Use the standard `drawFlightCard()` layout with these additions:
    - Header strip: add a `★` or `[PIN]` prefix to the callsign or replace the `N/M` counter with `★ PINNED` in the counter position.
    - Out-of-range position: if `distance_km` is valid, show distance and cardinal bearing in the status area even on an enriched card (currently hidden when enrichment is present) — e.g. `"1 240km NW"`. If `distance_km` is NaN, omit the distance line.
    - Full enrichment block: show route, airline, aircraft type, departure/arrival status lines and progress bar as normal when AeroAPI data is available.
    - No-position AeroAPI-only card: show `"Not yet airborne"` or `"Arrived — {city}"` in place of the live-metrics fallback rows.
    - Map card: if the pinned flight has a valid position it appears on the radar map as normal; if out of radar range it is excluded from the map overlay (no off-screen dot).

  **Credit cost:** one additional OpenSky bounding-box-equivalent credit per fetch cycle (the targeted `callsign=` query) plus one AeroAPI call — same marginal cost as adding one more in-range aircraft. On the free OpenSky tier (~1 000 credits/day at 30 s intervals) this leaves ~950 credits/day for the normal radius query.

  **WebUI:** pinned flight card appears first in the scrolling flight panel with a `★ PINNED` badge; out-of-range flights show a muted `{N} km away` sub-line beneath the route.

- **Touch input on CYD XPT2046** — currently unused; could drive card-next / pause / detail overlay
- **Per-airline accent colour** — FlightWall CDN dropped `brand_color_hex`; either embed a small static palette by ICAO or compute a dominant colour from the cached JPEG logo so cards stop being uniformly white
- **Eliminate Save & Reboot for most WebUI settings** — the reboot was added as a safe default, but most settings do not actually require one. Analysis of what each field needs:

  | Setting | Current state | Can go live without reboot? |
  |:--------|:-------------|:----------------------------|
  | Latitude / Longitude / Radius | `RuntimeConfig` in-memory setters applied immediately in `onPostConfig`; `FlightDataFetcher` reads them each fetch; `MapProvider::ensureMapCached()` detects coordinate changes via NVS comparison and auto-invalidates the map tile on the next fetch cycle | **Yes — already live** |
  | Fetch interval (`fetch_sec`) | Read from `RuntimeConfig::fetchIntervalSec()` in `loop()` every tick | **Yes — already live** |
  | Card cycle / Map display sec | Read from `RuntimeConfig` inside `CYDDisplay::displayFlights()` every call | **Yes — already live** |
  | Map label colour (`lbl_col`) | Read from `RuntimeConfig::labelColor()` in `drawMapCard()` on every render | **Yes — live on next map render; call `g_display.resetRenderState()` in `onPostConfig` to force immediate pickup** |
  | OpenSky client ID / secret | Read from `RuntimeConfig` in `OpenSkyFetcher` on each token refresh | **Yes — live on next OAuth cycle; no token cache to flush explicitly** |
  | AeroAPI key | Read from `RuntimeConfig` in `AeroAPIFetcher` on every call | **Yes — already live** |
  | Backlight brightness | Set once via `ledcWrite` during `CYDDisplay::initialize()` — not re-applied dynamically | **Needs a `ledcWrite(TFT_BL, brightness)` call from `onPostConfig` (or a `CYDDisplay::applyBrightness()` helper called by `main.cpp` after a config flag)** |
  | WiFi SSID / Password | Managed by WiFiManager in its own NVS namespace — cannot be changed live; requires captive-portal AP which only opens at boot | **Always needs reboot** |

  Implementation path: remove `_pendingReboot` from `onPostConfig` for non-WiFi fields; add a brightness-apply step (expose a `RuntimeConfig`-aware `ledcWrite` call or a `CYDDisplay::applyBrightness()` helper); call `g_display.resetRenderState()` so colour changes are picked up immediately; change the POST response from `"Rebooting..."` to `"Settings saved"`; keep reboot only when WiFi credentials are submitted (detect by non-empty `ssid` or `pass` fields in the POST body, or add a dedicated WiFi-credentials sub-form).

- **WiFi settings reset from WebUI** — there is currently no way to clear stored WiFiManager credentials without physical access to reset flash. A "Reset WiFi" button in the Device Configuration panel should:
  1. Send `POST /api/resetwifi` to the device.
  2. The handler calls `WiFi.disconnect(true, true)` (erases stored SSID/password from the ESP32 NVS WiFi partition without needing the `WiFiManager` instance in scope) then sets the reboot flag.
  3. `main.cpp` reboots after the 400 ms TCP flush, and on next boot WiFiManager opens the **FlightWall-Setup** captive portal as on first boot.
  4. The WebUI should display a clear warning before the button is activated ("This will erase your WiFi credentials and reboot into setup mode") — a JS `confirm()` dialog is sufficient.

  Note: the `WiFiManager wm` instance is currently local to `initWiFi()` so `wm.resetSettings()` cannot be called later. Using `WiFi.disconnect(true, true)` from the handler avoids needing to hoist `wm` to file scope, and has the same effect (clears the ESP32 `nvs` WiFi namespace). Alternative: lift `wm` to a file-scope static in `main.cpp` to allow calling `wm.resetSettings()` from anywhere if future features need it.

---

## Code optimisations (future revisions)

- [ ] `FlightWallFetcher::getAirlineData` — remove the dead `brand_color_hex` parsing path; the CDN no longer returns the field and `airline_color` is always the 0xFFFF white fallback
- [ ] `FlightDataFetcher::fetchFlights` — hoist `FlightWallFetcher fw` out of the per-flight loop and consider reusing a single `WiFiClientSecure` across airline/aircraft/logo calls to amortise TLS handshakes
- [ ] `FlightWallFetcher::getAirlineLogo` — simplify the duplicate-skip clause `(i == 1 && code == airlineIcao)`; an `if (codes[i] == codes[i-1]) continue;` is clearer
- [ ] Replace `client.setInsecure()` (AeroAPI + FlightWall + logo proxy + Google Maps) with pinned CA root certificates or fingerprint validation for production
- [ ] `AeroAPIFetcher` — consider migrating to ArduinoJson v7 streaming or a tighter custom parser to drop the 16 KB `DynamicJsonDocument` (largest single heap allocation in the request path)
- [ ] `WebUIServer::onGetLive` — currently allocates a 32 KB `DynamicJsonDocument` plus a `String` serialisation buffer (raised from 24 KB in v0.14.0 to accommodate the uncapped flights array); stream JSON directly to the response or use `WebServer::sendContent()` in chunks
- [ ] `WebUIServer::onGetLogo` — restrict the `name` argument to `[A-Z0-9]{2,4}\.jpg` rather than the looser current check (`indexOf('/')`, `..`, `.jpg` suffix); reduces attack surface even on LAN
- [ ] `WebUIServer::onGetMapPreview` — apply the same path-validation tightening as `onGetLogo`; currently serves `/mapcache.jpg` directly but the file argument should still be allowlisted
- [ ] Add LRU / size cap to `/logos/` LittleFS cache so a long-lived device with diverse traffic does not exhaust the 384 KB SPIFFS partition (worst case ~80 logos at ~3 KB each leaves no room for `/mapcache.jpg`)
- [ ] `parseIso8601` — fold the two `sscanf` calls into a single pass; minor but it runs per record per fetch cycle
- [ ] `RuntimeConfig::save` — write only changed keys to reduce NVS wear on configuration POSTs
- [ ] `OpenSkyFetcher` token expiry uses `millis()` — survives ~49-day wrap by arithmetic but triggers an unnecessary refresh at wrap; switch to `time(nullptr)` after NTP sync
- [ ] Watchdog — enable the task / interrupt watchdog and feed it from the main loop so a hung HTTP call resets rather than wedges
- [ ] `cyd_480x320` build flags carry a "verify TFT_BL pin and SPI_FREQUENCY against your exact board revision" comment from initial bring-up; verify on actual ST7796 hardware and remove the warning
- [ ] Touch calibration — `TFT_eSPI`/XPT2046 calibration matrix should be persisted in NVS (`Preferences`) before touch features land
- [ ] Encapsulate `g_flights` / `g_states` / `g_emptyMessage` globals behind a small `AppState` class to make ownership and threading expectations explicit
- [ ] AeroAPI request budget (`MAX_AEROAPI_CALLS_PER_CYCLE = 10`) is compile-time; expose via `RuntimeConfig` and the WebUI for users on different paid tiers
- [ ] `CYDDisplay::displayFlights` render-skip uses a string-concatenated `renderKey`; replace with a hashed key (e.g. FNV-1a over the same fields) to remove the `String` allocation per loop tick
- [ ] `CYDDisplay::drawMapCard` collision-avoidance buffer is a 16-element stack array; raise or make dynamic if `MAX_AEROAPI_CALLS_PER_CYCLE` is ever exposed as runtime config above 16
- [ ] OpenSky `fetchStateVectors` — when WiFi reconnects mid-cycle, retry once instead of returning empty and forcing the next interval to refetch
- [ ] AeroAPI 401/403 are handled as generic non-200; treat 401 as a credential failure event surfaced in the WebUI Activity Feed
- [ ] `MapProvider` — extract the bare `http.GET()` / `writeToStream()` path into a shared helper with `FlightWallFetcher::httpGetToFile()`; near-duplicate code today
- [ ] `FlightDataFetcher::fetchFlights` clears `outStates` / `outFlights` unconditionally; consider `swap` with a temporary so callers' last-good vectors are never observed empty mid-fetch
- [ ] Reduce HTML/JS PROGMEM footprint by minifying `HTML_PAGE` at build time (raw size has grown past ~15 KB with the SVG map overlay, colour picker, flights toggle, map mirror and countdown)
- [ ] Dead code — `CYDDisplay::showStandaloneMap()` is still declared and defined but no longer called since the Preview Map button was removed in v1.2.0; safe to delete along with its declaration
- [ ] **WebUI responsiveness — async fetch on Core 0 (FreeRTOS task)**: move `FlightDataFetcher::fetchFlights()` and `MapProvider::ensureMapCached()` onto a task pinned to Core 0, leaving the WebServer and TFT loop on Core 1. Currently a fetch cycle blocks `server.handleClient()` for 30–150 s (1 OpenSky + up to 10 AeroAPI calls @ ~10–15 s each + FlightWall CDN + logo fetches), so the dashboard appears frozen. Requires a `SemaphoreHandle_t` around `g_flights` / `g_states` / `g_emptyMessage`, careful task-local `WiFiClientSecure` heap (~40 KB per task at peak — validate headroom against current 32 KB JSON + 16 KB AeroAPI doc), and a clean shutdown path on reboot. Supersedes the interleave-based mitigation once implemented; the `busy`/`phase` JSON fields and busy banner from that mitigation remain useful (set `busy=true` while the task holds the data mutex).
- [ ] **Watchdog timer** (also referenced above): once the FreeRTOS-task option lands, a genuine hung HTTPS call still appears identical to "busy" on the dashboard. Enabling the task / interrupt watchdog and feeding it from the main loop converts a true hang into a clean reboot rather than an indefinite "Awaiting response…" banner.
