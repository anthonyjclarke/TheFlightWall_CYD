# TheFlightWall Firmware — CYD Edition

PlatformIO firmware for the CYD (TFT) build target of [TheFlightWall OSS](https://github.com/AxisNimble/TheFlightWall_OSS) — the open-source flight wall project by [AxisNimble](https://github.com/AxisNimble). This repository implements the ESP32 "Cheap Yellow Display" hardware variant of that project using either 320×240 or 480×320 models.

> New to the CYD? See [Appendix A — Hardware background](#appendix-a--hardware-background-the-cheap-yellow-display) at the end of this document for board variants, where to buy, 3D-printable cases and community resources.

**What this edition adds over the base OSS spec:**

- **TFT display** — ILI9341 / ST7796 driver via TFT_eSPI; cycling flight cards with callsign, route, status lines, elapsed-time progress bar and cached airline logo
- **Google Maps card** — road-map JPEG fetched from the Google Maps Static API, cached 24 h on LittleFS, with all tracked aircraft overlaid as dots with heading ticks and labels
- **AeroAPI enrichment** — per-callsign route, origin/destination, operator and ISO 8601 timing data from FlightAware AeroAPI; graceful ADS-B-only fallback when unavailable
- **Airline branding** — friendly display names from the FlightWall CDN; logo JPEGs fetched via images.weserv.nl and cached permanently in LittleFS
- **Embedded web dashboard** — browser-rendered TFT mirror, scrolling activity feed, enriched-flight detail panel and runtime configuration form served from the device
- **Runtime configuration** — location, radius, fetch interval, display timing and API credentials persisted to NVS via the dashboard; no reflash needed
- **WiFiManager provisioning** — captive-portal AP on first boot; credentials stored in NVS

Current release: **v1.2.0** (29 May 2026) · dev branch building toward **v1.3.0**

> ![Hero shot of CYD running FlightWall](images/hero.png)
> 
> *TheFlightWall CYD running live — enriched flight cards cycling on the ILI9341 320×240 TFT.*

---

## Current status

- Default build target: `cyd_320x240`
- Verified build: `platformio run`
- WiFi provisioning: WiFiManager captive portal named **FlightWall-Setup**
- Flight position source: OpenSky Network OAuth2 REST API
- Flight route/aircraft enrichment: FlightAware AeroAPI
- Friendly airline/aircraft display names: FlightWall CDN JSON lookup files
- Airline logos: Jxck-S/airline-logos via the images.weserv.nl PNG→JPEG proxy, cached in LittleFS
- Display behaviour: cached flight list cycles independently of the network fetch interval; last slot in each cycle is a Google Maps card showing all tracked aircraft on a real road map; a status bar overlays the TFT during fetch cycles showing the active phase
- Web dashboard: browser-rendered TFT mirror, volatile live feed, enriched-flight detail panel, runtime configuration with configurable map label colour
- Diagnostic output: local Australia/Sydney timestamps after NTP sync; `[boot +Ns]` before sync
- Live no-extra-cost metrics from OpenSky: distance, bearing, altitude/flight level, speed, heading, climb/descent, ground state

---

## What it does

- Fetches nearby ADS-B state vectors from OpenSky Network using OAuth2 and a bounding-box query around your configured location.
- Enriches callsigns with route, aircraft type and operator details via FlightAware AeroAPI, selecting the live record from any historical/future legs returned for the same callsign.
- Looks up friendly airline and aircraft display names from the FlightWall CDN.
- Caches airline logo JPEGs on LittleFS, fetched from the Jxck-S/airline-logos repository via a PNG→JPEG image proxy.
- Renders cycling flight cards on CYD TFT displays, followed by a Google Maps card showing all tracked aircraft overlaid on a real road map centred on the configured location.
- Caches the map JPEG on LittleFS for 24 hours; re-fetches only when location or radius changes or the cache expires.
- Serves an embedded operational dashboard at the device IP address.
- Keeps an in-memory 50-entry scrolling log of the latest fetch results.
- Shows live ADS-B metrics without adding API cost: distance/cardinal bearing, altitude or flight level, speed, heading, climb/descent, ground state.

![Data pipeline diagram](images/pipeline.png)
*Data flow: OpenSky ADS-B state vectors → callsign validation in `FlightDataFetcher` → AeroAPI route enrichment → FlightWall CDN name and logo lookup → cycling TFT cards and HTTP dashboard.*

---

## Key components

| Path | Role |
|:-----|:-----|
| `src/main.cpp` | Entry point — WiFiManager provisioning, millis-based fetch loop, reboot scheduling |
| `src/core/FlightDataFetcher` | Orchestrates: state vectors → flight metadata → name enrichment → ADS-B fallback cards |
| `src/adapters/OpenSkyFetcher` | OpenSky OAuth2, bounding-box query, distance/bearing filter |
| `src/adapters/AeroAPIFetcher` | AeroAPI `/flights/{ident}` — route, aircraft, operator, ISO 8601 → UTC timing |
| `src/adapters/FlightWallFetcher` | CDN airline/aircraft display-name lookup; cached LittleFS logo retrieval |
| `src/adapters/CYDDisplay` | TFT_eSPI flight card — callsign, route, status lines, progress bar, JPEG logo; map card slot |
| `src/adapters/MapProvider` | Google Maps Static API fetch, LittleFS JPEG cache (24 h TTL), Web Mercator lat/lon → pixel projection |
| `src/adapters/WebUIServer` | HTTP server (port 80) — dashboard, JSON API, logo serving, runtime configuration |
| `src/config/` | `UserConfiguration`, `APIConfiguration`, `TimingConfiguration`, `HardwareConfiguration`, `RuntimeConfig` |
| `src/interfaces/` | `BaseDisplay`, `BaseFlightFetcher`, `BaseStateVectorFetcher` |
| `src/models/` | `FlightInfo`, `StateVector`, `AirportInfo` |
| `src/utils/GeoUtils.h` | Haversine distance and bearing calculations |
| `include/debug.h` | Leveled, locally timestamped macros: `DBG_ERROR` / `DBG_WARN` / `DBG_INFO` / `DBG_VERBOSE` |

---

## Quick-start

```bash
cp include/secrets.h.template include/secrets.h
```

Fill in `include/secrets.h`:

```cpp
#define SECRET_OPENSKY_CLIENT_ID     "your-opensky-api-client-id"
#define SECRET_OPENSKY_CLIENT_SECRET "your-opensky-api-client-secret"
#define SECRET_AEROAPI_KEY           "your-flightaware-aeroapi-key"
#define SECRET_MAPS_API_KEY          "your-google-maps-static-api-key"
```

Then set your location in `src/config/UserConfiguration.h` (or leave the defaults and override at runtime via the WebUI), select your environment in PlatformIO, and upload:

- `cyd_320x240` — ESP32-2432S028R (ILI9341, standard CYD)
- `cyd_480x320` — ESP32-3248S035R (ST7796, larger CYD)

On first boot the device opens an AP named **FlightWall-Setup** — connect from any device and enter your WiFi credentials.

---

## Display output

Each flight card follows the TheFlightWall OSS display layout. Layout (320×240, landscape):

| Zone | Content |
|:-----|:--------|
| Top bar | Large callsign centered; card position (`3/11`) at left |
| Airline column (left ~118 px) | Cached JPEG airline logo (`/logos/{CODE}.jpg`) if available; airline display name in white as fallback |
| Route column (right) | IATA origin → destination in amber (`LAX-JFK`); ICAO fallback when IATA is absent |
| Aircraft row | Aircraft type short name (e.g. `737-800`) |
| Status row 1 | "Departed Sydney 45 min ago" — once NTP sync is confirmed and AeroAPI returned a departure |
| Status row 2 | "Arriving at Melbourne in 4h 30m" or "Arrived at Melbourne" |
| Progress bar | Green fill proportional to elapsed flight time; hidden until NTP sync |
| ADS-B fallback row(s) | When enrichment is absent: `15km NE`, altitude / flight-level, speed, heading, climb/descent |
| **Map card** | Last slot in each cycle — Google Maps road map JPEG (cached 24 h) with flight dots, heading ticks and callsign labels overlaid via Web Mercator projection; flight count shown top-right |
| **Fetch status bar** | Amber-on-navy strip at very bottom of screen during API cycles; shows current phase (`"OpenSky"`, `"AeroAPI 3/10"`, `"Map cache"`, etc.); clears automatically when fetch completes |

> ![Map card on CYD TFT](images/map-card.jpg)
> 
> *Map card — Google Maps road-map JPEG with aircraft markers, heading ticks and amber callsign labels overlaid via Web Mercator projection.*

### Enrichment criteria

A flight card is **enriched** when AeroAPI successfully matched a live flight record for its callsign and returned route, operator, aircraft type, and ISO 8601 departure/arrival times. The card shows the route, airline, aircraft type, departure/arrival status lines and a progress bar.

A card is **ADS-B only** when enrichment was not attempted or returned no live match — for example: AeroAPI rate-limited, no API key configured, no current scheduled record found, or the callsign belongs to a private, charter, or military aircraft not covered by AeroAPI's commercial flight database. These cards still display with live ADS-B data: ident, altitude, speed, heading, distance and bearing.

Cards are suppressed from the TFT and the dashboard mirror entirely when they would be empty:

- Aircraft on the ground without an active AeroAPI match (parked / taxiing — e.g. a Jetstar 320 sitting at a SYD gate between rotations)
- Transponder targets with no callsign — the ident would be raw hex like `7cf4b0` (ground vehicles, helicopters, military targets with squelched ACID)

Airborne, validly-callsigned aircraft that do not match an AeroAPI record still cycle on the TFT as ADS-B-only cards — they are never suppressed.

When state vectors are received but every observation is filtered out, the TFT shows `No active flights within Nkm` (using the runtime radius) instead of an empty card or stale last-good. The display cycle is independent of network fetching — if a fetch is slow or returns no results at all, the display keeps cycling the last good flight list rather than freezing or blanking.

> ![Enriched flight card close-up](images/card-enriched.jpg)
> 
> *Enriched card: airline, IATA route (origin → destination), aircraft type, departure/arrival status lines, and elapsed-time progress bar.*

> ![ADS-B-only fallback card](images/card-adsb.jpg)
> *ADS-B-only card: live position data (distance, bearing, altitude, speed, heading, climb/descent) without route enrichment.*

> ![CYD in use on a flight wall](images/cyd-in-use.jpg)
> *CYD mounted on a flight wall — cards cycling automatically every three seconds.*

### Airline brand colour

Earlier releases used a `brand_color_hex` field returned by the FlightWall CDN to render the airline name in its livery colour. The CDN response no longer carries this field; the brand-colour code path is retained as dead code so airline names currently render in white (`COLOR_AIRLINE`). The cached JPEG logo, when present, is the primary visual airline cue.

---

## Web dashboard

Once WiFi is connected, open `http://<device-ip>/` in a browser. The dashboard is embedded in firmware as a single `PROGMEM` HTML/JS blob with no external dependencies and polls the device's JSON endpoints.

| Panel | Behaviour |
|:------|:----------|
| TFT Mirror | Browser-rendered replica of the currently selected display card, synchronised with the card index shown on the TFT. Flight cards render from JSON for low SPI/network cost; when the CYD enters its map slot the mirror swaps to the cached `/api/mappreview` JPEG with the same SVG flight-marker overlay used in the Device Configuration preview. An amber **● MAP CARD** pill in the panel header confirms the current state. A "⬇ Screenshot (BMP)" link in the footer downloads a pixel-perfect framebuffer dump via `GET /api/screenshot`. Logos are fetched via `GET /api/logo?name=...`. |
| Header status | `● Device live HH:MM:SS \| Next update Ns \| N API credits`. The countdown is driven by a `next_fetch_in` field on `/api/live` computed from `millis()` arithmetic so it is immune to browser/device clock skew. Reads `First fetch pending` during the 8 s startup grace and `Fetch in progress` while a cycle is busy. A pulsing amber banner sits directly below the header during fetch cycles, showing the active phase (`OpenSky`, `AeroAPI 3/10`, `Airline logo`, etc.). |
| Flight Data Feed | Scrolling feed of fetch-cycle events and live aircraft observations. Stores up to 50 entries in RAM only; clears on reboot. |
| Current Flights | Horizontally scrollable card per `g_flights` entry (no five-flight cap since v0.14.0). Callsign resolution matches the CYD (`ident_iata` → `ident` → `ident_icao`), so for example Virgin Australia's `VA804` shows as `VA804` on both the TFT and the dashboard. |
| Device Configuration | Runtime location, timing, brightness, **map label colour** (with hover ⓘ tooltip), and API credential updates stored in NVS with save-and-reboot behaviour. A "Fetch Map" button re-fetches the map tile for a candidate centre/radius without committing to NVS. |

Dashboard endpoints:

| Endpoint | Purpose |
|:---------|:--------|
| `GET /` | Embedded dashboard application (HTML in `WebUIServer.cpp` as `HTML_PAGE` PROGMEM blob) |
| `GET /api/live` | Current screen selection, up-to-five enriched flights, volatile activity feed, last-fetch epoch |
| `GET /api/logo?name=<file>.jpg` | Cached LittleFS airline logo image for the dashboard mirror |
| `GET /api/config` | Non-sensitive runtime configuration; `opensky_configured` and `aero_configured` boolean flags only |
| `POST /api/config` | Persist runtime settings; blank credential fields preserve the stored value. Sets a reboot flag handled by `main.cpp`. |
| `POST /api/fetchmap` | Updates centre/radius in memory only, re-fetches the map tile, and triggers CYD preview — used to validate coordinates before saving |
| `GET /api/mappreview` | Streams the cached `mapcache.jpg` from LittleFS for the browser map preview panel |
| `GET /api/screenshot` | Streams a 24-bit BMP of the live CYD framebuffer (RGB888 readback via `TFT_eSPI::readRectRGB`); timestamped attachment filename |

Credentials are write-only in the WebUI: stored OpenSky secrets and AeroAPI keys are never returned by `GET /api/config`.

> ![Web dashboard overview](images/webui-dashboard.png)
> *Web dashboard at `http://<device-ip>/` — TFT mirror, current flights panel with horizontal scroll, live activity feed, and runtime status.*

> ![WebUI configuration form](images/webui-config.png)
> *Configuration panel — location, fetch interval, display cycle, brightness, map label colour (with hover tooltip) and API credentials; saved to NVS with an automatic reboot. "Fetch Map" validates a candidate centre/radius by re-fetching the map tile without committing to NVS.*

---

## API services

The firmware uses five external services. Three require credentials; two are free with no sign-up needed.

| Service | Used for | Credential | Cost |
|:--------|:---------|:-----------|:-----|
| OpenSky Network REST API | Live ADS-B state vectors | OAuth2 client ID + secret | Credit-metered, free tier |
| FlightAware AeroAPI | Route, aircraft, operator enrichment | API key | Usage-based, free credit |
| Google Maps Static API | Map card background image | API key | $2 / 1 000 requests — free tier covers ~100 K/month |
| FlightWall CDN | Friendly airline / aircraft display names | None | Free |
| images.weserv.nl | PNG→JPEG proxy for airline logos | None | Free |

Full setup details, pricing, runtime behaviour, hints and tips: see [README-API.md](README-API.md).

Never commit `include/secrets.h`. It is intentionally gitignored; commit only `include/secrets.h.template`.

### OpenSky Network — essential setup

OpenSky uses OAuth2 client credentials (not username/password) for the REST API.

1. Sign in at <https://opensky-network.org/my-opensky/account>.
2. Find the **API Client** card and create a new client.
3. Copy `client_id` and `client_secret` into `include/secrets.h`:

```cpp
#define SECRET_OPENSKY_CLIENT_ID     "xxxxxxxx-api-client"
#define SECRET_OPENSKY_CLIENT_SECRET "xxxxxxxx"
```

Keep `RADIUS_KM` at 15 km or less and leave `FETCH_INTERVAL_SECONDS` at 30 s or higher to stay within the free daily credit allocation on an always-on device.

### FlightAware AeroAPI — essential setup

AeroAPI enriches each callsign with route, operator and timing data. Leaving the key blank runs the device in ADS-B-only mode (fully functional, no route data).

1. Sign up and select a tier at <https://www.flightaware.com/commercial/aeroapi>.
2. Copy your key from the developer portal at <https://www.flightaware.com/aeroapi/portal/>.
3. Add it to `include/secrets.h`:

```cpp
#define SECRET_AEROAPI_KEY "your-aeroapi-key"
```

Each `/flights/{ident}` call costs one request unit. With 5–10 aircraft in a 15 km urban radius at 30 s intervals, expect roughly 1 000–2 000 calls per day.

### Google Maps Static API — essential setup

The map card downloads a single road-map JPEG once every 24 hours (or when location/radius changes) and stores it on LittleFS. Leaving the key blank shows "Map unavailable" in the map card slot.

1. Open Google Cloud Console: <https://console.cloud.google.com/>.
2. Enable **Maps Static API** under **APIs & Services → Library**.
3. Create an API key under **APIs & Services → Credentials**.
4. Add it to `include/secrets.h`:

```cpp
#define SECRET_MAPS_API_KEY "your-google-maps-static-api-key"
```

A billing account must be attached, but Google's $200/month free credit covers ~100 000 map requests — far more than this device will use. Restrict the key to the Maps Static API to limit exposure.

---

## Notes

- OpenSky OAuth token is managed automatically with a 60-second refresh skew.
- AeroAPI enrichment runs per-callsign; each unique flight costs at most one API call per fetch cycle, capped at `MAX_AEROAPI_CALLS_PER_CYCLE` (10 in v0.14.0+) total calls per cycle.
- `FETCH_INTERVAL_SECONDS` controls OpenSky polling and downstream enrichment frequency; tune for your API quotas.
- `DISPLAY_CYCLE_SECONDS` controls how long each individual flight card stays on screen.
- `DISPLAY_MAP_SECONDS` (default 15 s) controls how long the map card stays on screen. Configurable at runtime via the WebUI "Map display sec" field.
- Debug output is controlled via the `-DDEBUG_LEVEL=N` build flag (default 3 = INFO) and switches to local Australia/Sydney timestamps after NTP sync.
- LittleFS is initialised at boot with `LittleFS.begin(true)` — the partition is formatted on first boot.

---

## Acknowledgements

This firmware is a CYD build target of [TheFlightWall OSS](https://github.com/AxisNimble/TheFlightWall_OSS), the open-source flight wall project by [AxisNimble](https://github.com/AxisNimble).

### Libraries

| Library | Author | Licence | Purpose |
|:--------|:-------|:--------|:--------|
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | Bodmer | MIT | TFT display driver and graphics primitives |
| [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder) | Bodmer | MIT | On-device JPEG decode and render via TJpgDec |
| [WiFiManager](https://github.com/tzapu/WiFiManager) | tzapu | MIT | Captive-portal WiFi provisioning |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | Benoît Blanchon | MIT | JSON parsing for all API responses (v6) |

### Data and asset sources

| Source | URL | Used for |
|:-------|:----|:---------|
| OpenSky Network | <https://opensky-network.org> | Live ADS-B state vectors |
| FlightAware AeroAPI | <https://www.flightaware.com/commercial/aeroapi> | Flight route and operator enrichment |
| Google Maps Static API | <https://developers.google.com/maps/documentation/maps-static> | Road-map JPEG for map card background |
| FlightWall CDN | <https://cdn.theflightwall.com> | Airline and aircraft friendly display names |
| Jxck-S/airline-logos | <https://github.com/Jxck-S/airline-logos> | Airline logo PNG images (~1 800 airlines by ICAO code) |
| images.weserv.nl | <https://images.weserv.nl> | PNG→JPEG conversion and resize proxy; result cached to LittleFS on first fetch |

---

## Appendix A — Hardware background: the Cheap Yellow Display

### What is a CYD?

The **Cheap Yellow Display** (CYD) is a family of low-cost ESP32 development boards manufactured primarily by Sunton, distinguished by an integrated TFT touchscreen on a yellow PCB. The name was coined by **[Brian Lough](https://github.com/witnessmenow)** (`@witnessmenow`), whose [ESP32-Cheap-Yellow-Display repository](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) is the de facto reference: pin maps, library configurations, sample code, comparison tables and a community-maintained list of board revisions.

A typical CYD bundles:

- An **ESP32-WROOM-32 module** (dual-core 240 MHz, 4 MB flash, 320 KB RAM, WiFi + Bluetooth)
- A **TFT display** with resistive or capacitive touch
- A **microSD slot**
- An **RGB LED** (on most models)
- A **CdS LDR** (light sensor)
- A **CP2102** or **CH340** USB-to-serial bridge
- An **I²S audio output** on later revisions
- Two **JST connectors** exposing spare GPIOs for peripherals

Retail prices sit between **US $7 and US $25** depending on screen size and seller, which is what put it on the hobbyist map — comparable bare ESP32 + TFT combos typically cost twice as much without the integrated extras.

### Common board variants

The yellow PCB now spans a whole family of screen sizes and controllers. The two targeted by this firmware are marked **★**:

| Sunton SKU              | Screen          | Resolution | Driver         | Touch       | USB     | Notes                                  |
|:------------------------|:----------------|:-----------|:---------------|:------------|:--------|:---------------------------------------|
| **ESP32-2432S028R** ★   | 2.8″ SPI TFT    | 320 × 240  | ILI9341        | Resistive (XPT2046) | Micro-USB | The original CYD; the "R" suffix means resistive touch. A "C" suffix variant (`-028C`) uses capacitive touch and a slightly different pinout. |
| **ESP32-3248S035R** ★   | 3.5″ SPI TFT    | 480 × 320  | ST7796 *or* ILI9488 | Resistive (XPT2046) | Micro-USB or USB-C | Two driver chips ship under the same SKU — check yours; this firmware default-builds for ST7796 (`-DST7796_DRIVER=1`). |
| ESP32-2432S028          | 2.8″ SPI TFT    | 320 × 240  | ILI9341        | None        | Micro-USB | Touch-less variant.                    |
| ESP32-4827S043          | 4.3″ Parallel   | 480 × 272  | RGB parallel   | Capacitive  | USB-C   | Higher pin-count; needs PSRAM.         |
| ESP32-8048S043          | 4.3″ Parallel   | 800 × 480  | RGB parallel   | Capacitive  | USB-C   | Requires ESP32-S3 + PSRAM variants.    |
| ESP32-8048S050          | 5.0″ Parallel   | 800 × 480  | RGB parallel   | Capacitive  | USB-C   |                                        |
| ESP32-8048S070          | 7.0″ Parallel   | 800 × 480  | RGB parallel   | Capacitive  | USB-C   | Largest in the family.                 |

> ⚠ **Pin maps differ between revisions.** Even within the same SKU, manufacturers occasionally re-spin the board with different TFT_BL pins, swapped MISO routing, or substituted display controllers. **Always check the markings on your actual PCB against [witnessmenow's reference table](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display#variants)** before relying on a pin assignment.

### Where to buy

CYDs are sold through the usual hobbyist channels — none of these are endorsements, just where the community typically sources them:

- **AliExpress** — searches for `ESP32-2432S028R`, `ESP32 Cheap Yellow Display`, or `Sunton ESP32 LCD` return the widest selection; expect 2–4 week delivery. *(Tip: the Sunton official store generally ships current revisions; cheap unbranded resellers sometimes ship older or untested batches.)*
- **Amazon** — usually 2–3× the AliExpress price; faster delivery and easier returns.
- **eBay** — frequent listings from EU and US sellers with shorter shipping windows.
- **Sunton's own listings** on [Alibaba](https://www.alibaba.com/) for bulk orders (10+ units).

Always confirm the **exact SKU** in the listing description — vendors often photograph the original `-2432S028R` even when shipping a different variant.

### 3D-printable cases and stands

Several community-designed enclosures are well-suited to a flight-wall mounting:

- **[FlightWall ESP32 2.8″ CYD enclosure](https://www.thingiverse.com/thing:6440252)** on Thingiverse — purpose-designed for the 320 × 240 CYD running flight tracking firmware; tidy front bezel and rear cable relief.
- **[CYD 2.8″ Desk Stand](https://www.thingiverse.com/thing:6427338)** and many other variants searchable as `Cheap Yellow Display` on [Thingiverse](https://www.thingiverse.com/search?q=cheap+yellow+display) and [Printables](https://www.printables.com/search/models?q=cheap+yellow+display).
- **MakerWorld**, **Cults3D** and **Thangs** carry community remixes for the 3.5″ and larger variants.

For the 480 × 320 (`ESP32-3248S035R`) target, search specifically for `ESP32-3248` to avoid 2.8″ enclosures that don't match the larger bezel.

### Community resources

- **[witnessmenow / ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)** — Brian Lough's canonical reference repo. Start here. Pin diagrams, working TFT_eSPI configurations, library notes, video walkthroughs.
- **[The CYD Discord channel](https://discord.com/channels/630078152038809611/1109228361441620028)** — part of Brian Lough's Discord server; active community for troubleshooting board-specific quirks, sharing builds and asking about revision differences. *(You'll need to be a member of Brian's Discord server to access the channel.)*
- **[Brian Lough's YouTube channel](https://www.youtube.com/@witnessmenow)** — long-form video tutorials on getting the CYD running from scratch.
- **[Random Nerd Tutorials — ESP32 Cheap Yellow Display](https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/)** by Rui Santos — beginner-friendly pin-out reference and first-flash walkthrough.
- **[Volos Projects — CYD playlist](https://www.youtube.com/@VolosProjects)** — Volos has published several CYD projects (clocks, dashboards, retro game emulators) that are good starting points for new buyers.
- **[r/esp32 on Reddit](https://www.reddit.com/r/esp32/)** — general ESP32 community; search "CYD" or "Cheap Yellow Display" for build threads and troubleshooting.

### Known quirks worth noting

These come up frequently in the community and are worth knowing before you start:

- **MISO (GPIO 12) is unwired on some board revisions.** TFT framebuffer readback (`tft.readPixel`, `tft.readRectRGB`) requires MISO. This firmware's `GET /api/screenshot` endpoint depends on it; if you get all-white screenshots, your board variant likely lacks the MISO trace.
- **GPIO 12 is an ESP32 strapping pin** (MTDI). Using it as MISO is normally safe — the ILI9341 tri-states MISO when CS is high — but if your board has a strong pull-up on the MISO line, the chip can fail to boot. Symptom: bootloop after enabling `-DTFT_MISO=12`.
- **Touch SPI is a separate bus** (XPT2046 on GPIO 33 CS) running at a different clock rate (typically 2.5 MHz) than the display SPI. Don't try to share buses.
- **The microSD slot shares SPI with the display.** Card initialisation must happen after the display is set up, with explicit CS management on GPIO 5.
- **`GPIO 0` (boot) shares the on-board "BOOT" button** with the screen backlight on some revisions — pressing BOOT for entry into flash mode can briefly flicker the screen.
- **USB connector type is not a reliable revision indicator.** Both micro-USB and USB-C versions of the same SKU exist; the silicon and PCB layout are the determining factors.

### Credits

The CYD acronym and the bulk of the community knowledge base are the work of **Brian Lough** ([`@witnessmenow`](https://github.com/witnessmenow)). The board family itself is manufactured by **Sunton**. Pin maps, comparison tables and revision histories used in this appendix draw heavily from the [ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) repository's community contributors.
