# TheFlightWall Firmware — CYD Edition

PlatformIO firmware for the CYD (TFT) variant of TheFlightWall. The current root-level project targets ESP32 "Cheap Yellow Display" boards rather than the original LED matrix build.

---

## Current status

- Default build target: `cyd_320x240`
- Verified build: `platformio run` / `/Users/anthonyjclarke/.platformio/penv/bin/platformio run`
- WiFi provisioning: WiFiManager captive portal named **FlightWall-Setup**
- Flight position source: OpenSky Network OAuth2 REST API
- Flight route/aircraft enrichment: FlightAware AeroAPI
- Friendly airline/aircraft labels: FlightWall CDN lookup files
- Display behavior: cached flight list cycles independently of the network fetch interval
- Live no-extra-cost metrics shown from OpenSky: distance, bearing, altitude/flight level, speed, heading, climb/descent, and ground state

The legacy `NeoMatrixDisplay` adapter is retained for reference, but CYD builds explicitly exclude it.

---

## What it does

- Fetches nearby ADS-B state vectors from OpenSky Network using OAuth2 and a bounding-box query around your configured location.
- Enriches callsigns with route, aircraft type, and operator details via FlightAware AeroAPI.
- Looks up friendly airline and aircraft display names from the FlightWall CDN.
- Renders cycling flight cards on CYD TFT displays.
- Shows live ADS-B metrics without adding API cost:
  distance/cardinal bearing, altitude or flight level, speed, heading, climb/descent, and ground state.

---

## Key components

| Path | Role |
|:-----|:-----|
| `src/main.cpp` | Entry point — WiFiManager provisioning, millis-based fetch loop |
| `core/FlightDataFetcher` | Orchestrates: state vectors → flight metadata → name enrichment |
| `adapters/OpenSkyFetcher` | OpenSky OAuth2, bounding-box query, geo filter |
| `adapters/AeroAPIFetcher` | AeroAPI `/flights/{ident}` — route, aircraft, operator |
| `adapters/FlightWallFetcher` | CDN airline/aircraft display-name lookup |
| `adapters/CYDDisplay` | TFT_eSPI flight card — header, route, live metrics, sub-info footer |
| `adapters/NeoMatrixDisplay` | Original LED matrix renderer (legacy, retained for reference) |
| `config/` | `UserConfiguration`, `APIConfiguration`, `TimingConfiguration`, `HardwareConfiguration` |
| `include/debug.h` | Leveled macros: `DBG_ERROR` / `DBG_WARN` / `DBG_INFO` / `DBG_VERBOSE` |
| `include/secrets.h.template` | Template for API credentials — copy to `secrets.h` (gitignored) |

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

Then set your location in `config/UserConfiguration.h`, select your environment in PlatformIO, and upload:

- `cyd_320x240` — ESP32-2432S028R (ILI9341, standard CYD)
- `cyd_480x320` — ESP32-3248S035R (ST7796, larger CYD)

On first boot the device opens an AP named **FlightWall-Setup** — connect from any device and enter your WiFi credentials.

---

## Display output

Each enriched flight card displays:

- Airline/operator name, or operator code when no friendly lookup exists
- Card position, for example `3/11`
- Origin and destination ICAO codes from AeroAPI
- Live distance and cardinal direction from `CENTER_LAT` / `CENTER_LON`
- Altitude or flight level from OpenSky barometric/geometric altitude
- Ground speed, heading, and climb/descent indicator from OpenSky
- Callsign and aircraft type/name

Example compact 320x240 card content:

```text
QantasLink        3/11
YSSY  >  YSCB
18km SW  7200ft  420km/h DN 084deg
QLK1449                 Dash 8-400
```

The display cycle is independent of network fetching. If a fetch is slow, rate-limited, or returns no enriched flights, the display keeps cycling the last good flight list instead of freezing on the fetch result or blanking immediately.

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

The state-vector request is a bounding-box query around `UserConfiguration::CENTER_LAT`, `CENTER_LON`, and `RADIUS_KM`:

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

### Testing credentials

Use `DEBUG_LEVEL=3` or `DEBUG_LEVEL=4` in `platformio.ini` while bringing up credentials. Useful serial messages:

- `OpenSky: token obtained` means OAuth succeeded.
- `OpenSky: token POST failed` usually means the OpenSky client ID/secret is wrong, missing, or copied from the wrong OpenSky interface.
- `AeroAPI: no API key configured` means `SECRET_AEROAPI_KEY` is blank or `include/secrets.h` was not found.
- `AeroAPI: HTTP 401` or `403` usually means the AeroAPI key is invalid, disabled, or not authorised for the endpoint.
- `AeroAPI: HTTP 429` indicates rate limiting or exhausted quota.

If `include/secrets.h` exists but credentials still appear blank, check that it is under `include/`, not the project root, and that each `#define` is above any accidental `#ifdef` or comment block.

---

## Notes

- OpenSky OAuth token is managed automatically with a 60-second refresh skew.
- AeroAPI enrichment runs per-callsign; each unique flight costs one API call per fetch cycle.
- `FETCH_INTERVAL_SECONDS` controls both OpenSky polling and downstream enrichment frequency; tune it for your API quotas.
- `DISPLAY_CYCLE_SECONDS` controls how long each cached flight card stays on screen.
- Debug output is controlled via the `-DDEBUG_LEVEL=N` build flag (default 3 = INFO).
