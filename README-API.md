# FlightWall CYD — API Reference

Full setup, pricing, runtime behaviour, and debugging notes for every external service used by the firmware.

---

## How the pipeline works

The firmware combines five external data sources with an on-device orchestrator and two-tier cache.
Each fetch cycle (default 30 s) follows the same flow:

1. **OpenSky Network** is queried for live ADS-B state vectors inside a bounding box around the
   configured centre point. A bearer token is exchanged once per session via OAuth2 client
   credentials and refreshed automatically before expiry. The raw response is filtered by
   great-circle distance and sorted closest-first.
2. **`FlightDataFetcher` (on-device)** validates each callsign — rejecting 1–2-letter ICAO prefixes,
   pure-alpha callsigns and callsigns with embedded spaces — strips ATC duplicate-departure suffixes
   (e.g. `QLK423D` → `QLK423`) and copies the ADS-B state (distance, bearing, altitude, speed,
   heading, vertical rate, ground state, squawk) into a `FlightInfo` record.
3. **FlightAware AeroAPI** is called per valid callsign up to a per-cycle cap (default 10) to retrieve
   route, operator, aircraft type, actual departure and estimated arrival. Only plausible live records
   are kept; historical legs are rejected. Cards without a live match remain visible as ADS-B-only.
4. **FlightWall CDN** supplies friendly airline and aircraft display names keyed by ICAO code
   (`QFA` → "Qantas"; `B738` → "Boeing 737-800"). No credentials required.
5. **Jxck-S/airline-logos** (~1 800 airline PNGs hosted on GitHub) is the source of airline logo
   imagery. PNGs are routed through **images.weserv.nl** which re-encodes to JPEG and resizes to
   80×80 in a single request — TJpg_Decoder on the ESP32 supports baseline JPEG only. The result is
   cached to LittleFS as `/logos/{CODE}.jpg` and served from cache on subsequent boots.
6. **Google Maps Static API** provides the road-map background JPEG for the map card. Like the
   logos, it is routed through **images.weserv.nl** because Google returns progressive JPEG which
   TJpg_Decoder cannot decode (`JDR_FMT3`). The map is cached in LittleFS as `/mapcache.jpg` for
   24 hours and auto-invalidates when centre or radius changes.

The resulting `g_flights` vector (plus the two LittleFS-cached JPEGs and the NVS runtime config)
drives both outputs: **CYDDisplay** cycles flight cards (3 s) and a radar/map slot (15 s) on the TFT,
while **WebUIServer** mirrors the same data through an HTTP dashboard on port 80 with a 50-entry
volatile activity feed.

![Data pipeline](images/pipeline.png)

---

## Services at a glance

| Service                   | Used for                                  | Credentials needed       | Cost model              |
|:--------------------------|:------------------------------------------|:-------------------------|:------------------------|
| OpenSky Network REST API  | Live ADS-B state vectors                  | OAuth2 client ID + secret| Credit-metered, free tier |
| FlightAware AeroAPI       | Route, aircraft, operator enrichment      | API key                  | Usage-based, free credit |
| Google Maps Static API    | Map card background image                 | API key                  | $2 / 1 000 requests     |
| FlightWall CDN            | Friendly airline/aircraft display names   | None                     | Free                    |
| images.weserv.nl          | PNG→JPEG proxy for airline logos          | None                     | Free                    |

---

## OpenSky Network

### What it does in this project

`OpenSkyFetcher` calls `/api/states/all` with a bounding-box query around the configured centre point and radius. Each response returns the raw ADS-B state for every transponder OpenSky has observed within that box in the last 15 seconds. `FlightDataFetcher` then filters by great-circle distance, discards ground-only transponders with no callsign, and passes the accepted entries to AeroAPI enrichment.

The following fields come from OpenSky at no extra cost and are displayed directly on flight cards: distance, bearing, barometric/geometric altitude, ground speed, heading, vertical rate, ground state, ICAO24 address, squawk and position source.

### Credentials

OpenSky no longer accepts website username/password for the REST API. You need an **API Client** (OAuth2 client credentials), which is separate from your login:

1. Sign in or register at <https://opensky-network.org/my-opensky/account>.
2. On the account page, find the **API Client** card and create a new client.
3. Copy the `client_id` and `client_secret` into `include/secrets.h`:

```cpp
#define SECRET_OPENSKY_CLIENT_ID     "xxxxxxxx-api-client"
#define SECRET_OPENSKY_CLIENT_SECRET "xxxxxxxx"
```

### Pricing and free tier

OpenSky uses a credit system. Authenticated users receive a daily credit allocation; `/states/all` costs credits proportional to the bounding-box size — a tight 15 km radius around a city uses far fewer credits than a global query. Credits reset daily. If you exhaust your daily allocation the API returns `429 Too Many Requests` and the firmware retains the last-good flight list until the next cycle succeeds.

Reducing `FETCH_INTERVAL_SECONDS` (default 30 s) and `RADIUS_KM` (default 15 km) are the two most effective ways to stay within the free credit allocation on an always-on device.

### Runtime behaviour

- The firmware exchanges client credentials for a bearer token at the OpenSky OAuth endpoint once per session and refreshes automatically with a 60-second safety margin before expiry.
- If OpenSky returns `401 Unauthorized`, the token is refreshed once and the state-vector request is retried immediately.
- Both the token POST and the state-vector GET use a 15 s HTTP timeout (`http.setTimeout(15000)`). The default 5 s timeout produces `-11 HTTPC_ERROR_READ_TIMEOUT` on slow OpenSky responses — do not reduce this value.
- State vectors are sorted by distance; the closest aircraft are always preferred for AeroAPI enrichment and TFT display.

### Hints and tips

- Keeping `RADIUS_KM` at 15 km or less is usually sufficient for interesting traffic around a city airport and minimises credit consumption.
- An API client created under one OpenSky account cannot be shared; create one per device if running multiple units.
- OpenSky data lags real-world ADS-B by approximately 5–15 seconds due to aggregation latency.
- `DEBUG_LEVEL=3` logs `OpenSky: raw states=N in_radius=M` after every fetch, which is the fastest way to confirm the bounding box is working.

### Useful links

- REST API docs: <https://openskynetwork.github.io/opensky-api/rest.html>
- Account and API Client page: <https://opensky-network.org/my-opensky/account>
- OpenSky FAQ: <https://opensky-network.org/about/faq>

---

## FlightAware AeroAPI

### What it does in this project

For each callsign accepted by `FlightDataFetcher`, `AeroAPIFetcher` calls `GET /flights/{ident}` and selects a plausible live record from the response. It populates the `FlightInfo` struct with origin, destination, operator, aircraft type, actual departure epoch and estimated arrival epoch. These fields drive the route display (`LAX–JFK`), the status lines ("Departed 45 min ago / Arriving in 2 h 30 m") and the elapsed-time progress bar on each flight card.

If AeroAPI is unavailable or returns no active record for a callsign, that flight remains visible as an ADS-B-only card using only OpenSky data. AeroAPI enrichment is never required for a card to display.

### Credentials

1. Sign up and choose a tier at <https://www.flightaware.com/commercial/aeroapi>.
2. After signup, open the developer portal at <https://www.flightaware.com/aeroapi/portal/> and copy your API key.
3. Add it to `include/secrets.h`:

```cpp
#define SECRET_AEROAPI_KEY "your-aeroapi-key"
```

### Pricing and free tier

AeroAPI is usage-based. FlightAware provides a small free monthly credit (verify the current amount at the portal) which is sufficient for low-volume testing and occasional use. An always-on device fetching at 30 s intervals can exhaust the free credit quickly; each `/flights/{ident}` call costs one request unit.

The firmware caps enrichment at `MAX_AEROAPI_CALLS_PER_CYCLE` (default 10, configurable at compile time) per 30-second cycle. With 5–10 aircraft typically visible in a 15 km urban radius, this usually means one AeroAPI call per aircraft per 30 seconds of flight activity — roughly 1 000–2 000 calls per day for a busy city location.

To control costs: increase `FETCH_INTERVAL_SECONDS`, reduce `MAX_AEROAPI_CALLS_PER_CYCLE`, or leave `SECRET_AEROAPI_KEY` blank to run in ADS-B-only mode. The display is still useful without AeroAPI enrichment.

### Runtime behaviour

- AeroAPI responses can contain multiple flight records (historical, current and future legs). The firmware selects only a plausible live record: in-progress, arrived within 30 minutes, or departed with no known arrival time. Historical records (> 70 hours old) are rejected and the aircraft falls back to an ADS-B-only card.
- AeroAPI timestamps carry timezone offsets (e.g. `2026-05-25T08:00:00+10:00`). `parseIso8601` converts these to UTC epochs independently of the local timezone configured for serial debug output.
- The firmware uses a `StaticJsonDocument<768>` filter and a `DynamicJsonDocument<16384>` main document. Do not reduce the main document size — unfiltered AeroAPI responses cause `NoMemory` parse failures on-device.
- `http.getString()` is used (not `getStream()`) to buffer the full response body; `getStream()` delivers zero bytes to ArduinoJson on ESP32-Arduino 3.x with `WiFiClientSecure`.

### Hints and tips

- `HTTP 429` from AeroAPI means rate-limited or quota exhausted. The firmware continues displaying the last enriched list.
- `HTTP 401` or `403` usually means the key is invalid, not yet active, or not authorised for the endpoint.
- If you see `AeroAPI: <ident> no active match in N records`, ADS-B is working but all route records are stale — often occurs 30–60 minutes after a flight lands. The ADS-B-only card continues to show until the transponder goes quiet.
- A callsign like `QFA123` may return 3–5 historical legs for the same flight number. The firmware's record-selection logic is designed to pick the current operation, not a previous day's flight with the same number.
- Testing without an AeroAPI key is straightforward: leave `SECRET_AEROAPI_KEY` blank and all cards will display as ADS-B-only. OpenSky provides enough data to confirm the display, WiFi, location filter and cycling behaviour.

### Useful links

- AeroAPI product and pricing: <https://www.flightaware.com/commercial/aeroapi>
- AeroAPI developer portal: <https://www.flightaware.com/aeroapi/portal/>
- AeroAPI help centre: <https://support.flightaware.com/hc/en-us/sections/32586090657175-AeroAPI>

---

## Google Maps Static API

### What it does in this project

`MapProvider` fetches a single road-map JPEG image sized to the TFT resolution (e.g. 320×240 for the standard CYD) centred on the configured location and zoomed to fit the search radius. The image is stored in LittleFS as `/mapcache.jpg` and reused for 24 hours, or until the configured centre or radius changes. This JPEG is drawn as the full-screen background of the map card in the display cycle, with flight markers overlaid using Web Mercator projection.

The map card appears as the last slot in each rotation. It uses a separate dwell timer (`map_sec`, default 15 s) so it remains visible longer than individual flight cards.

### Credentials

1. Open Google Cloud Console: <https://console.cloud.google.com/>.
2. Create or select a project.
3. Navigate to **APIs & Services → Library** and enable **Maps Static API**.
4. Navigate to **APIs & Services → Credentials** and create an **API key**.
5. (Optional but recommended) Restrict the key to the Maps Static API to limit exposure.
6. Add the key to `include/secrets.h`:

```cpp
#define SECRET_MAPS_API_KEY "your-google-maps-static-api-key"
```

A billing account must be attached to the project, but Google provides $200/month of free Maps credit — far more than this device will consume.

### Pricing and free tier

The Maps Static API costs $2 per 1 000 requests. The $200/month free credit equates to 100 000 free requests per month. With the 24-hour on-device cache, a single CYD makes at most ~30 map requests per month (one per day plus occasional cache invalidations when location changes). Running costs for the map card are effectively zero under the free tier.

To avoid surprise charges: restrict the API key to the Maps Static API and to the device's IP range if possible. Google also allows a monthly spending cap in the billing console.

### Runtime behaviour

- The map is fetched in the flight-fetch cycle (every `fetch_sec` seconds by default), not at display time. If WiFi is unavailable or the key is missing, the map card shows "Map unavailable" until a successful fetch occurs.
- The cache is invalidated and re-fetched when centre latitude, centre longitude, or radius changes (i.e. after a WebUI config save and reboot) or after 24 hours.
- The request is made over HTTPS using `WiFiClientSecure`; the response is written directly to LittleFS via `http.writeToStream()` to avoid allocating the full JPEG on the heap.
- Zoom level is chosen automatically from `radius_km` and screen dimensions. Reducing `radius_km` increases zoom (more street detail); increasing it zooms out.
- `MapProvider::mapVersion()` is a monotonic counter bumped on each download. `CYDDisplay::mapRenderKey()` includes this value so a freshly-fetched map forces a display redraw.

### Hints and tips

- If the map card shows "Map unavailable" after adding the key, check the serial log for `MapProvider: HTTP 403` (key restrictions too tight) or `MapProvider: no API key` (key not defined in `secrets.h`).
- The map is a standard Google road map (`maptype=roadmap`). This can be changed to `satellite`, `terrain`, or `hybrid` in `APIConfiguration::MAPS_STATIC_URL` at compile time.
- Flight markers appear as amber dots (enriched flights) or cyan dots (ADS-B-only), each with a short heading tick. Labels are truncated to 7 characters. Flights beyond the map boundary are not drawn.
- On the 320×240 display with a 15 km radius, the auto-selected zoom level is approximately 9–10, which shows roughly 40 km of road context around the centre.

---

## FlightWall CDN

### What it does in this project

`FlightWallFetcher` retrieves friendly display names for airlines and aircraft from the FlightWall CDN. These replace raw ICAO codes with human-readable strings like "Qantas" and "Boeing 737-800" on flight cards.

```text
GET https://cdn.theflightwall.com/oss/lookup/airline/{ICAO}.json
GET https://cdn.theflightwall.com/oss/lookup/aircraft/{ICAO}.json
```

No credentials are required. If a lookup fails, the firmware falls back to the operator code or aircraft code returned by AeroAPI.

### Current CDN response shape (verified May 2026)

```json
{ "icao": "QFA", "display_name_full": "Qantas" }
{ "icao": "B738", "display_name_full": "Boeing 737-800", "display_name_short": "737-800" }
```

The CDN no longer returns `brand_color_hex`; airline names render in white when no cached JPEG logo is available.

---

## Airline logos — images.weserv.nl

### What it does in this project

`FlightWallFetcher` fetches airline logos from the public [Jxck-S/airline-logos](https://github.com/Jxck-S/airline-logos) GitHub repository (~1 800 airlines by ICAO code). PNGs are converted to JPEG and resized to 80×80 px by the `images.weserv.nl` proxy in a single request, then cached to LittleFS as `/logos/{CODE}.jpg`. Subsequent boots serve directly from LittleFS with no network call.

No credentials are required for either service. `images.weserv.nl` is the same proxy used for map tile rendering in earlier implementations; it is already a trusted dependency in this project.

---

## Diagnostic messages

Use `-DDEBUG_LEVEL=3` (default) or `-DDEBUG_LEVEL=4` in `platformio.ini` to enable detailed serial logging. All messages include a local Australia/Sydney timestamp after NTP sync, or `[boot +Ns]` before sync.

### OpenSky

| Message | Meaning |
|:--------|:--------|
| `RuntimeConfig APIs: OpenSky=configured` | Credentials populated — values not logged |
| `OpenSky: token obtained` | OAuth2 exchange succeeded |
| `OpenSky: token POST failed` | Client ID/secret wrong, missing, or from the wrong OpenSky interface |
| `OpenSky: query center=... bbox=...` | Confirms the bounding box being sent |
| `OpenSky: raw states=N in_radius=M` | N total responses, M within the radius filter |
| `OpenSky: HTTP GET failed, code: -11` | Read timeout — do not reduce the 15 s timeout |

### AeroAPI

| Message | Meaning |
|:--------|:--------|
| `AeroAPI: no API key configured` | `SECRET_AEROAPI_KEY` blank or `secrets.h` not found |
| `AeroAPI: HTTP 401` / `403` | Key invalid, disabled or not authorised |
| `AeroAPI: HTTP 429` | Rate-limited or quota exhausted |
| `AeroAPI: <ident> no active match in N records` | All route records are historical/stale — ADS-B-only card retained |
| `AeroAPI: <ident> dep="..." delta=...min` | Confirms the selected record and departure timing |

### Google Maps

| Message | Meaning |
|:--------|:--------|
| `MapProvider: cache valid` | 24 h cache hit — no network call |
| `MapProvider: fetching 320x240 zoom=10` | Cache miss — downloading new map |
| `MapProvider: cached N bytes (v=M)` | Download succeeded; `v` = map version counter |
| `MapProvider: no API key` | `SECRET_MAPS_API_KEY` blank or `secrets.h` not found |
| `MapProvider: HTTP 403` | Key restrictions too tight or API not enabled |

### General

- `FlightData: states=N cards=M aero_calls=K enriched=J` — distinguishes missing aircraft from missing enrichment. If `states=0` and `cards=0`, OpenSky returned nothing in the radius.
- `RuntimeConfig loaded: ...` — confirms all runtime values including `map_sec` and `cycle_sec`.
- If `secrets.h` exists but credentials appear blank, verify the file is under `include/` (not the project root) and that each `#define` appears before any accidental `#ifdef` guard.
