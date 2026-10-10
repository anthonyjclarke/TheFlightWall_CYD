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
|                     | `cyd_480x320` – 4.0″ ESP32-32E (E32R40T)       |
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
| 1.5.0-dev CI (`47e83c2`), 2.8″ / 4.0″ | 1,275,280 B / 1,281,904 B  |

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

## Release check – Update from the live page (case 2)

**Passed 10-10-2026** on the same board (MAC `b0:cb:d8:da:ae:8c`), live page
<https://anthonyjclarke.github.io/TheFlightWall_CYD/> serving v1.5.0 (tag run
38028710087). The release has both boards' `*-firmware.bin` and
`*-merged.bin` plus `SHA256SUMS.txt`; every manifest lists four parts.

1. The board ran the `1.5.0-dev` preview from the smoke test. Brightness was
   set from 200 to 150 through `POST /api/config` as a marker.
2. Connect on the live page offered **Update TheFlightWall_CYD**, with no
   erase question, and the Update completed.
3. After the Update the boot log shows `v1.5.0`, `Running from app0` and
   `bright=150`. WiFi rejoined from saved credentials and NTP synced.
   `/api/config` still reports brightness 150 and the dashboard title reads
   `v1.5.0`.

## 4.0″ board – install from the live page (case 1)

**Passed 11-10-2026** – 4.0″ ESP32-32E (LCDwiki E32R40T), ESP32-D0WD-V3
rev 3.1, MAC `a4:f0:0f:68:95:5c`, installed from the live v1.5.0 page. This
board was listed as a 3.5″ ESP32-3248S035R until 1.5.1; the image and env
(`cyd_480x320`) are unchanged.

The boot log shows `v1.5.0`, `Running from app0`, `CYDDisplay init: 480x320
px`, WiFi joined (`192.168.1.82`) and NTP synced, with no crash. Settings
were at factory defaults and there was no splash, as on a fresh install; the
erase itself wasn't separately confirmed. The Improv name is
`FlightWall-F0A4`.

### Update to 1.5.1 on the 4.0″

**Passed 11-10-2026** on the same board, from the live page serving v1.5.1
(tag run 38070600610), which now labels it *CYD 4.0″ – ESP32-32E
(E32R40T)*. After the Update the boot log shows `v1.5.1`, `Running from
app0`, `480x320 px`, WiFi rejoined from saved credentials and NTP synced,
with no crash. Its settings were still at defaults, so only WiFi showed
that NVS was kept; the 2.8″ Update above proved settings survive.

---

## Tests owed

Smoke-tested only. Run these on the next real work on this project, or before
the next release, and tick them off with date and board MAC.

- [x] Case 1 – fresh install on the 4.0″ ESP32-32E (E32R40T) – 11-10-2026, `a4:f0:0f:68:95:5c`
- [x] Case 2 – Update on a provisioned board (settings kept) – 10-10-2026, `b0:cb:d8:da:ae:8c`
- [x] Case 3 – Update from `app1` – not applicable, no OTA
- [ ] Case 4 – wrong board image, then reinstall (2.8″ ↔ 4.0″)
- [ ] Keys entered in the dashboard: OpenSky fetch, AeroAPI, Maps background
