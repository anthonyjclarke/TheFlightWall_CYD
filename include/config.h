#pragma once

// ─── Firmware identity and web installer contract ────────────────────────────
//
// The cyd-web-installer CI reads FIRMWARE_VERSION, PROJECT_NAME and the
// optional strings below from this file. They stay #define string literals
// because WebUIServer pastes the version into PROGMEM HTML (see Version.h,
// which aliases FW_VERSION_STR to FIRMWARE_VERSION).
//
// PROJECT_NAME is frozen: Improv reports it and the installer manifest uses it
// as `name`. Renaming it turns every "Update" into an erase-prompting "Install".
//
// Suffix conventions:
//   • "1.5.0-dev"   work-in-progress on the dev branch
//   • "1.5.0"       tagged release on main (CI refuses a suffix on a tag)
// ─────────────────────────────────────────────────────────────────────────────

#define FIRMWARE_VERSION "1.6.0-dev"
#define PROJECT_NAME     "TheFlightWall_CYD"  // frozen: Improv + manifest name

#define UPSTREAM_PROJECT  "TheFlightWall_OSS"
#define UPSTREAM_AUTHOR   "AxisNimble"
#define UPSTREAM_REPO_URL "https://github.com/AxisNimble/TheFlightWall_OSS"

// WiFiManager captive-portal AP name (first-boot provisioning)
#define AP_NAME "FlightWall-Setup"

// Improv-Serial WiFi setup and "Update" detection for the browser installer
#define IMPROV_SETUP_ENABLED 1
#define IMPROV_DEVICE_PREFIX "FlightWall"
