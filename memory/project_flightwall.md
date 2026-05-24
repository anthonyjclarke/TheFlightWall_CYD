---
name: project-flightwall-oss
description: FlightWall OSS project context — hardware targets, phase plan, architecture notes
metadata:
  type: project
---

FlightWall OSS (TheFlightWall_OSS) is now rooted as a PlatformIO CYD TFT firmware project.
The active build target uses CYDDisplay with TFT_eSPI; NeoMatrixDisplay remains in the tree only
as legacy/reference code and is excluded from CYD builds. Phase 2 will add a WebUI for all
configuration.

**Why:** User wants wall-mounted ESP32 display (commercial product at www.FlightPanel.com) to
support the cheaper, more widely available CYD hardware instead of custom LED panels.

**Hardware targets:**
- `cyd_320x240` — ESP32-2432S028R, ILI9341, 320×240 (standard "Cheap Yellow Display")
- `cyd_480x320` — ESP32-3248S035R, ST7796, 480×320 (larger CYD variant); TFT_BL=27 — needs verification

**Architecture (current):**
- Root-level PlatformIO project; old `firmware/` layout has been superseded
- `adapters/CYDDisplay` — TFT_eSPI renderer, GFX free fonts, cached flight card cycling
- `adapters/OpenSkyFetcher` — OAuth2 ADS-B state vectors
- `adapters/AeroAPIFetcher` — flight metadata; filtered JSON parse to reduce heap use
- `adapters/FlightWallFetcher` — CDN airline/aircraft name lookup
- `core/FlightDataFetcher` — orchestrates fetch pipeline
- `adapters/NeoMatrixDisplay` — legacy, kept in tree but excluded from CYD builds
- WiFiManager (tzapu) for WiFi provisioning; credentials in NVS
- Credentials in `include/secrets.h` (gitignored); template at `include/secrets.h.template`
- ArduinoJson v6 (`^6.21.0`) — original code used v6 API despite v7 in lib_deps
- `FlightInfo` includes live OpenSky state copied from each matching state vector:
  distance, bearing, altitude, speed, heading, vertical rate, and ground state
- Display cycling is independent of the network fetch interval and keeps the last good enriched
  flight list during empty/rate-limited fetches

**How to apply:** For any changes to the display, add to CYDDisplay. For Phase 2 WebUI,
add a `WebConfig` component that reads/writes Preferences NVS and serves config pages.
