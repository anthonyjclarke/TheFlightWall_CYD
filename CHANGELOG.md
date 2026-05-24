# Changelog

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
