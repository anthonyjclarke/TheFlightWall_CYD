# TheFlightWall Firmware — CYD Edition

PlatformIO firmware for the CYD (TFT) variant of TheFlightWall. The current root-level project targets ESP32 "Cheap Yellow Display" boards rather than the original LED matrix build.

Current release: **v0.13.0** (25 May 2026)

---

## Current status

- Default build target: `cyd_320x240`
- Verified build: `platformio run` / `/Users/anthonyjclarke/.platformio/penv/bin/platformio run`
- WiFi provisioning: WiFiManager captive portal named **FlightWall-Setup**
- Flight position source: OpenSky Network OAuth2 REST API
- Flight route/aircraft enrichment: FlightAware AeroAPI
- Friendly airline/aircraft labels: FlightWall CDN lookup files
- Display behavior: cached flight list cycles independently of the network fetch interval
- Web dashboard: browser-rendered TFT mirror, volatile live feed, enriched-flight detail and configuration
- Diagnostic output: local Australia/Sydney timestamps after NTP sync, boot elapsed time before sync
- Live no-extra-cost metrics shown from OpenSky: distance, bearing, altitude/flight level, speed, heading, climb/descent, and ground state

---

## What it does

- Fetches nearby ADS-B state vectors from OpenSky Network using OAuth2 and a bounding-box query around your configured location.
- Enriches callsigns with route, aircraft type, and operator details via FlightAware AeroAPI.
- Looks up friendly airline and aircraft display names from the FlightWall CDN.
- Renders cycling flight cards on CYD TFT displays.
- Serves an embedded operational dashboard at the device IP address.
- Keeps an in-memory scrolling log of the latest API-derived flight activity.
- Shows live ADS-B metrics without adding API cost:
  distance/cardinal bearing, altitude or flight level, speed, heading, climb/descent, and ground state.

---

## Key components

| Path | Role |
|:-----|:-----|
| `src/main.cpp` | Entry point — WiFiManager provisioning, millis-based fetch loop |
| `src/core/FlightDataFetcher` | Orchestrates: state vectors → flight metadata → name enrichment |
| `src/adapters/OpenSkyFetcher` | OpenSky OAuth2, bounding-box query, geo filter |
| `src/adapters/AeroAPIFetcher` | AeroAPI `/flights/{ident}` — route, aircraft, operator |
| `src/adapters/FlightWallFetcher` | CDN airline/aircraft display-name lookup |
| `src/adapters/CYDDisplay` | TFT_eSPI flight card — callsign, route, status lines, progress bar |
| `src/adapters/WebUIServer` | HTTP server (port 80) — live dashboard, JSON API, logo serving and runtime configuration |
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
```

Then set your location in `src/config/UserConfiguration.h`, select your environment in PlatformIO, and upload:

- `cyd_320x240` — ESP32-2432S028R (ILI9341, standard CYD)
- `cyd_480x320` — ESP32-3248S035R (ST7796, larger CYD)

On first boot the device opens an AP named **FlightWall-Setup** — connect from any device and enter your WiFi credentials.

---

## Display output

Each flight card is styled to match the commercial FlightWall product display. Layout (320×240):

| Zone | Content |
|:-----|:--------|
| Top bar | Large callsign on the left; card position (`3/11`) on the right |
| Airline column (left ~118 px) | JPEG airline logo (cached in LittleFS) if available; airline name in brand color otherwise |
| Route column (right) | IATA origin → destination in amber (`LAX → JFK`); ICAO fallback when IATA is absent |
| Aircraft row | Aircraft type short name (e.g. `A321neo`) |
| Status row | "Departed LAX 45 min ago" or "Arriving in 4h 30m" — shown once NTP sync is confirmed |
| Progress bar | Green fill proportional to elapsed flight time; hidden until NTP sync |

When AeroAPI enrichment is unavailable for a flight (rate-limited, no response, or API key absent), the card still displays using live ADS-B data only: callsign, altitude, speed, heading, distance, and bearing. Route and status lines are omitted for ADS-B-only cards.

The display cycle is independent of network fetching. If a fetch is slow, rate-limited, or returns no results, the display keeps cycling the last good flight list rather than freezing or blanking.

## Web dashboard

Once WiFi is connected, open `http://<device-ip>/` in a browser. The dashboard is embedded in firmware and has no external web dependencies.

| Panel | Behaviour |
|:------|:----------|
| TFT Mirror | Browser-rendered replica of the currently selected display card, synchronized with the card index shown on the TFT. It deliberately avoids reading back and transmitting literal framebuffer pixels, which would add SPI and network overhead to the ESP32 render loop. |
| Flight Data Feed | Scrolling feed of fetch results and live aircraft observations. It stores the last 50 entries in RAM only and clears on reboot. |
| Enriched Flight Intelligence | Up to five current AeroAPI-matched flights with route, operator, aircraft, schedule and extended ADS-B fields that do not fit on the TFT. |
| Device Configuration | Runtime location, timing, brightness and API credential updates stored in NVS with save-and-reboot behaviour. |

Dashboard endpoints:

| Endpoint | Purpose |
|:---------|:--------|
| `GET /` | Embedded dashboard application |
| `GET /api/live` | Current screen selection, enriched flights and volatile activity feed |
| `GET /api/logo?name=<file>.jpg` | Cached LittleFS airline-logo image for the mirror |
| `GET /api/config` | Non-sensitive runtime configuration and credential-configured flags |
| `POST /api/config` | Persist runtime settings and reboot; blank credential fields preserve the stored value unless explicitly cleared |

Credentials are write-only in the WebUI: stored OpenSky secrets and AeroAPI keys are never returned by `GET /api/config`.

---

## API authorisations

The firmware uses three network data sources:

| Service | Used for | Credential needed | Config field |
|:--------|:---------|:------------------|:-------------|
| OpenSky Network REST API | Nearby live ADS-B state vectors from `/api/states/all` | OAuth2 API client ID and secret | `SECRET_OPENSKY_CLIENT_ID`, `SECRET_OPENSKY_CLIENT_SECRET` |
| FlightAware AeroAPI | Route, origin/destination, operator, and aircraft metadata for each callsign | AeroAPI key | `SECRET_AEROAPI_KEY` |
| FlightWall CDN | Friendly airline and aircraft display-name lookups | None | No secret required |

Never commit `include/secrets.h`. It is intentionally gitignored; commit only `include/secrets.h.template`.

### OpenSky Network

This project uses OpenSky's REST API with OAuth2 client credentials. OpenSky no longer accepts website username/password basic auth for the REST API; you need an API Client from your OpenSky account.

1. Create or sign in to an OpenSky account:
   <https://opensky-network.org/my-opensky/account>
2. Open the account page and find the **API Client** card.
3. Create a new API client if one does not already exist.
4. Copy the generated `client_id` and `client_secret` into `include/secrets.h`:

```cpp
#define SECRET_OPENSKY_CLIENT_ID     "xxxxxxxx-api-client"
#define SECRET_OPENSKY_CLIENT_SECRET "xxxxxxxx"
```

At runtime `OpenSkyFetcher` exchanges those values for a bearer token at:

```text
https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token
```

The firmware sends:

```text
grant_type=client_credentials
client_id=<SECRET_OPENSKY_CLIENT_ID>
client_secret=<SECRET_OPENSKY_CLIENT_SECRET>
```

The returned access token is cached and refreshed automatically with a 60-second safety margin. If OpenSky returns `401 Unauthorized`, the firmware refreshes the token once and retries the state-vector request.

The state-vector request is a bounding-box query around runtime-configured center latitude, center longitude and radius (falling back to `UserConfiguration` defaults):

```text
https://opensky-network.org/api/states/all?lamin=...&lamax=...&lomin=...&lomax=...
```

OpenSky API credits are limited. Current OpenSky documentation lists `/states/all` as credit-metered, with standard authenticated users receiving a daily credit allocation and small bounding-box requests costing fewer credits than global requests. Keep `RADIUS_KM` tight and set `TimingConfiguration::FETCH_INTERVAL_SECONDS` conservatively for always-on devices.

Useful OpenSky references:

- REST API docs: <https://openskynetwork.github.io/opensky-api/rest.html>
- OpenSky FAQ auth section: <https://opensky-network.org/about/faq>
- Account/API Client page: <https://opensky-network.org/my-opensky/account>

### FlightAware AeroAPI

AeroAPI is used after OpenSky returns nearby aircraft. For each callsign, `AeroAPIFetcher` calls:

```text
GET https://aeroapi.flightaware.com/aeroapi/flights/{ident}
```

The key is sent as the `x-apikey` header. To get a key:

1. Go to FlightAware AeroAPI:
   <https://www.flightaware.com/commercial/aeroapi>
2. Choose a tier appropriate for your use. FlightAware lists usage-based pricing and free monthly credit on the AeroAPI product page; confirm the current pricing before running an always-on device.
3. Open the AeroAPI developer portal after signup:
   <https://www.flightaware.com/aeroapi/portal/>
4. Copy your API key into `include/secrets.h`:

```cpp
#define SECRET_AEROAPI_KEY "your-aeroapi-key"
```

Every enriched flight can cost an AeroAPI request. If five nearby aircraft have callsigns during one fetch cycle, the firmware may make up to five `/flights/{ident}` calls. Reduce AeroAPI usage by increasing `FETCH_INTERVAL_SECONDS`, reducing `RADIUS_KM`, or temporarily leaving `SECRET_AEROAPI_KEY` blank while testing OpenSky/display behavior.

The firmware parses only the fields it needs from the AeroAPI response to reduce ESP32 heap pressure. Large AeroAPI responses previously caused ArduinoJson `NoMemory` parse errors; the current parser uses a filter for route, operator, aircraft, and identifier fields.

An AeroAPI callsign can return historical and future records as well as the live flight. The firmware now selects only a record whose timing is plausible for the aircraft currently in range: an in-progress flight, a flight that arrived in the last 30 minutes, or a bounded no-arrival-time case. Historical responses such as a departure more than 70 hours old are rejected and the aircraft remains visible as an ADS-B-only card instead of displaying a false route.

`HTTP 429` means the AeroAPI key is valid but the account is being rate-limited or has exhausted quota. The firmware will continue displaying the last successful flight list, but fewer new flights may be enriched until quota recovers.

Useful AeroAPI references:

- AeroAPI product and pricing page: <https://www.flightaware.com/commercial/aeroapi>
- AeroAPI developer portal: <https://www.flightaware.com/aeroapi/portal/>
- FlightAware AeroAPI help: <https://support.flightaware.com/hc/en-us/sections/32586090657175-AeroAPI>

### FlightWall CDN lookups

`FlightWallFetcher` uses public JSON lookup files hosted at:

```text
https://cdn.theflightwall.com/oss/lookup/airline/{ICAO}.json
https://cdn.theflightwall.com/oss/lookup/aircraft/{ICAO}.json
```

No authorisation is required. These calls only add friendly display names. If a lookup fails, the display can still use the operator code or aircraft code returned by AeroAPI.

### OpenSky-only metrics

The following fields are taken from the OpenSky `/states/all` response and therefore do not add AeroAPI calls:

| Field | Display use |
|:------|:------------|
| `distance_km` | Distance from configured center point |
| `bearing_deg` | Cardinal direction from configured center point |
| `baro_altitude` / `geo_altitude` | Altitude or flight level |
| `velocity` | Ground speed in km/h |
| `heading` | Track/heading in degrees |
| `vertical_rate` | `UP` / `DN` climb/descent indicator |
| `on_ground` | `GROUND` status |
| `icao24`, `time_position`, `last_contact` | Identity and observation timing in the dashboard detail view |
| `latitude`, `longitude` | Last live position in the dashboard detail view |
| `squawk`, `position_source` | Transponder/source detail in the dashboard detail view |

### Testing credentials

Use `DEBUG_LEVEL=3` or `DEBUG_LEVEL=4` in `platformio.ini` while bringing up credentials. Useful serial messages:

- `[YYYY-MM-DD HH:MM:SS] [INFO] ...` confirms NTP synchronization and local Australia/Sydney debug timestamps; before sync messages use `[boot +Ns]`.
- `RuntimeConfig: OpenSky configured=... AeroAPI configured=...` confirms that credentials are populated without logging their values.
- `OpenSky: query center=... radius=... bbox=...` and `OpenSky: raw states=... in_radius=...` confirm the requested region and filter result.
- `FlightData: states=... cards=... aero_calls=... enriched=...` distinguishes missing live aircraft from missing enrichment.
- `OpenSky: token obtained` means OAuth succeeded.
- `OpenSky: token POST failed` usually means the OpenSky client ID/secret is wrong, missing, or copied from the wrong OpenSky interface.
- `AeroAPI: no API key configured` means `SECRET_AEROAPI_KEY` is blank or `include/secrets.h` was not found.
- `AeroAPI: HTTP 401` or `403` usually means the AeroAPI key is invalid, disabled, or not authorised for the endpoint.
- `AeroAPI: HTTP 429` indicates rate limiting or exhausted quota.
- `AeroAPI: <ident> no active match in ... records` means live ADS-B was received but all route records were stale or otherwise implausible; the ADS-B-only card remains available.

If `include/secrets.h` exists but credentials still appear blank, check that it is under `include/`, not the project root, and that each `#define` is above any accidental `#ifdef` or comment block.

---

## Notes

- OpenSky OAuth token is managed automatically with a 60-second refresh skew.
- AeroAPI enrichment runs per-callsign; each unique flight costs one API call per fetch cycle.
- `FETCH_INTERVAL_SECONDS` controls both OpenSky polling and downstream enrichment frequency; tune it for your API quotas.
- `DISPLAY_CYCLE_SECONDS` controls how long each cached flight card stays on screen.
- Debug output is controlled via the `-DDEBUG_LEVEL=N` build flag (default 3 = INFO) and switches to local Australia/Sydney timestamps after NTP sync.

---

## Acknowledgements

This firmware is derived from and inspired by the commercial [TheFlightWall](https://theflightwall.com) product. The CYD edition is an independent open-source reimplementation; it is not affiliated with or endorsed by TheFlightWall.

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
| FlightWall CDN | <https://cdn.theflightwall.com> | Airline and aircraft friendly display names, brand colors |
| Jxck-S/airline-logos | <https://github.com/Jxck-S/airline-logos> | Airline logo PNG images (~1 800 airlines by ICAO code) |
| images.weserv.nl | <https://images.weserv.nl> | PNG→JPEG conversion and resize proxy; cached to LittleFS on first fetch |
