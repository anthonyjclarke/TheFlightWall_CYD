# Web installer – TheFlightWall_CYD

The browser installer at <https://anthonyjclarke.github.io/TheFlightWall_CYD/>
is built by the shared tooling in
[cyd-web-installer](https://github.com/anthonyjclarke/cyd-web-installer). This
file records what is specific to this project and its hardware test results.

---

## Project specifics

| Item                | Value                                          |
|:--------------------|:-----------------------------------------------|
| `PROJECT_NAME`      | `TheFlightWall_CYD` (frozen)                   |
| Envs / boards       | `cyd_320x240` – 2.8″ ESP32-2432S028R           |
|                     | `cyd_480x320` – 3.5″ ESP32-3248S035R           |
| Version header      | `include/config.h` (`FW_VERSION_STR` aliases)  |
| Platform            | `espressif32@6.12.0` (arduino-esp32 2.0.17)    |
| Partitions          | `partitions_custom.csv`, app slot `0x1C0000`   |
| OTA                 | None – the `app1` case does not apply          |
| LittleFS            | Not shipped; splash optional (FS option C)     |
| Improv              | Core-0 task, `improvTick()` every 20 ms        |
| Setup AP            | `FlightWall-Setup`                             |
| Upstream credit     | TheFlightWall_OSS by AxisNimble                |

### Why a task for Improv

A fetch cycle runs HTTPS calls of up to 15 s each back to back (OpenSky,
AeroAPI per flight, FlightWall CDN, logos, map tile), and `setup()` holds the
splash, waits for WiFi and waits up to 10 s for NTP. Ticking Improv from
`loop()` would miss ESP Web Tools' 1.5 s Connect window during any of these,
so a FreeRTOS task on core 0 services it instead.

### No secrets in the image

CI builds without `include/secrets.h`, so the installer image has no API
keys. OpenSky, AeroAPI and Google Maps keys are entered in the dashboard and
stored in NVS (`osky_id`, `osky_sec`, `aero_key`, `maps_key`). The Maps key
field was added in 1.5.0 for this reason.

### No splash image

The installer writes four parts and never the LittleFS partition, so
`/splash.jpg` is absent after a fresh install. `CYDDisplay::showSplash()`
checks once per boot and draws a "FlightWall" text banner instead. Before
1.5.0 it re-checked and logged a warning on every loop pass in the empty
state. A board updated from 1.4.0 keeps the splash it already had.

### Settings across an Update

Settings are in NVS namespace `flightwall` and WiFi is in the ESP WiFi NVS, so
an Update (four parts, no erase) keeps both. Boards on 1.4.0 or older have no
Improv, so they're offered **Install**; answering **No** to erase keeps their
settings, because the partition table is unchanged.

### Build sizes

| Build                                 | `firmware.bin`             |
|:--------------------------------------|:---------------------------|
| 1.5.0-dev baseline, 6.12.0 pinned     | 1,264,448 B / 1,271,152 B  |
| 1.5.0-dev CI (`47e83c2`), 2.8″ / 3.5″ | 1,275,280 B / 1,281,904 B  |

Both are about 70 % of the 1,835,008 B slot. The unpinned platform resolved
to a pioarduino (arduino-esp32 3.x) build that no longer compiles.

---

## Smoke test (RUNBOOK 5a)

**Passed 10-10-2026** – CYD 2.8″ ESP32-2432S028R, ESP32-D0WD-V3 rev 3.1,
MAC `b0:cb:d8:da:ae:8c`, macOS desktop Chrome, CI preview of `47e83c2`
(`1.5.0-dev`) served from localhost.

| Step                                     | Result                              |
|:-----------------------------------------|:------------------------------------|
| CI Firmware run 38026413630              | Green; `build` OK, publish skipped  |
| `pio run -t erase`, then Install + erase | Four parts written, booted          |
| Configure WiFi (Improv)                  | Joined WiFi, restarted              |
| Connect again                            | "Connected to FlightWall-CBB0"      |
| Boot log after reset                     | `Running from app0`, WiFi, NTP OK   |
| No splash, no keys                       | Text banner; one warning per boot   |

Boot log (after the dialog was closed):

```text
[INFO] Improv: listening on Serial as FlightWall-CBB0
[INFO] FlightWall CYD firmware v1.5.0-dev booting
[INFO] Running from app0
[INFO] RuntimeConfig APIs: OpenSky=missing AeroAPI=missing Maps=missing
[INFO] LittleFS mounted — free: 376832 bytes
[WARN] showSplash: /splash.jpg not present in LittleFS — using text banner
*wm:Connecting to SAVED AP: …
[INFO] WiFi connected: 192.168.1.95
[INFO] NTP synced: epoch=1791609941 timezone=Australia/Sydney
[INFO] Free heap: 227956 bytes
[WARN] OpenSky: OAuth credentials not configured
[WARN] MapProvider: no Google Maps API key — enter one in the web UI
```

The Improv device name ends in `CBB0`, from the Espressif OUI rather than the
MAC tail. That comes from the shared `improv_setup.cpp` and is cosmetic.

---

## Tests owed

Smoke-tested only. Run these on the next real work on this project, or before
the next release, and tick them off with date and board MAC.

- [ ] Case 1 – fresh install, erased, on the 3.5″ ESP32-3248S035R
- [ ] Case 2 – Update on a provisioned board (settings kept)
- [x] Case 3 – Update from `app1` – not applicable, no OTA
- [ ] Case 4 – wrong board image, then reinstall (2.8″ ↔ 3.5″)
- [ ] Keys entered in the dashboard: OpenSky fetch, AeroAPI, Maps background
