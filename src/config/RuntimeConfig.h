#pragma once

#include <Arduino.h>

// NVS-backed runtime configuration (Preferences, namespace "flightwall").
// Call load() once in setup() before any fetcher or display is used.
// Compile-time defaults from UserConfiguration, TimingConfiguration,
// HardwareConfiguration, and APIConfiguration are used as fallback when no
// NVS value exists.  Call save() to persist in-memory changes.
namespace RuntimeConfig
{
  void load();
  void save();

  // Location
  double   centerLat();
  double   centerLon();
  double   radiusKm();

  // Timing
  uint32_t fetchIntervalSec();
  uint32_t displayCycleSec();
  uint32_t mapDisplaySec();

  // Display
  uint8_t  brightness();
  uint16_t labelColor();   // RGB565 — applied to enriched flight markers on both CYD and WebUI maps

  // API credentials
  String   openskyClientId();
  String   openskyClientSecret();
  String   aeroApiKey();

  // Setters (in-memory only; call save() to persist)
  void setCenterLat(double v);
  void setCenterLon(double v);
  void setRadiusKm(double v);
  void setFetchIntervalSec(uint32_t v);
  void setDisplayCycleSec(uint32_t v);
  void setMapDisplaySec(uint32_t v);
  void setBrightness(uint8_t v);
  void setLabelColor(uint16_t v);
  void setOpenskyClientId(const String &v);
  void setOpenskyClientSecret(const String &v);
  void setAeroApiKey(const String &v);
}
