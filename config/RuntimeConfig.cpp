#include "config/RuntimeConfig.h"

#include <Preferences.h>
#include "config/UserConfiguration.h"
#include "config/TimingConfiguration.h"
#include "config/HardwareConfiguration.h"
#include "config/APIConfiguration.h"
#include "debug.h"

// NVS namespace (max 15 chars)
static constexpr const char *NVS_NS = "flightwall";

// In-memory state — initialised to compile-time defaults
static double   s_centerLat;
static double   s_centerLon;
static double   s_radiusKm;
static uint32_t s_fetchSec;
static uint32_t s_cycleSec;
static uint8_t  s_brightness;
static String   s_openskyId;
static String   s_openskySecret;
static String   s_aeroApiKey;

void RuntimeConfig::load()
{
  Preferences p;
  p.begin(NVS_NS, true); // read-only

  s_centerLat  = p.getDouble("ctr_lat",    UserConfiguration::CENTER_LAT);
  s_centerLon  = p.getDouble("ctr_lon",    UserConfiguration::CENTER_LON);
  s_radiusKm   = p.getDouble("radius_km",  UserConfiguration::RADIUS_KM);
  s_fetchSec   = p.getUInt  ("fetch_sec",  TimingConfiguration::FETCH_INTERVAL_SECONDS);
  s_cycleSec   = p.getUInt  ("cycle_sec",  TimingConfiguration::DISPLAY_CYCLE_SECONDS);
  s_brightness = p.getUChar ("brightness", HardwareConfiguration::BL_BRIGHTNESS);

  // Credential fallback: NVS value → compile-time secret → empty
  s_openskyId     = p.getString("osky_id",  APIConfiguration::OPENSKY_CLIENT_ID);
  s_openskySecret = p.getString("osky_sec", APIConfiguration::OPENSKY_CLIENT_SECRET);
  s_aeroApiKey    = p.getString("aero_key", APIConfiguration::AEROAPI_KEY);

  p.end();

  DBG_INFO("RuntimeConfig loaded: lat=%.4f lon=%.4f r=%.0fkm fetch=%us cycle=%us bright=%u",
           s_centerLat, s_centerLon, s_radiusKm, s_fetchSec, s_cycleSec, s_brightness);
}

void RuntimeConfig::save()
{
  Preferences p;
  p.begin(NVS_NS, false); // read-write

  p.putDouble("ctr_lat",   s_centerLat);
  p.putDouble("ctr_lon",   s_centerLon);
  p.putDouble("radius_km", s_radiusKm);
  p.putUInt  ("fetch_sec", s_fetchSec);
  p.putUInt  ("cycle_sec", s_cycleSec);
  p.putUChar ("brightness",s_brightness);
  p.putString("osky_id",   s_openskyId);
  p.putString("osky_sec",  s_openskySecret);
  p.putString("aero_key",  s_aeroApiKey);

  p.end();
  DBG_INFO("RuntimeConfig saved to NVS");
}

// ── Getters ──────────────────────────────────────────────────────────────────
double   RuntimeConfig::centerLat()           { return s_centerLat; }
double   RuntimeConfig::centerLon()           { return s_centerLon; }
double   RuntimeConfig::radiusKm()            { return s_radiusKm; }
uint32_t RuntimeConfig::fetchIntervalSec()    { return s_fetchSec; }
uint32_t RuntimeConfig::displayCycleSec()     { return s_cycleSec; }
uint8_t  RuntimeConfig::brightness()          { return s_brightness; }
String   RuntimeConfig::openskyClientId()     { return s_openskyId; }
String   RuntimeConfig::openskyClientSecret() { return s_openskySecret; }
String   RuntimeConfig::aeroApiKey()          { return s_aeroApiKey; }

// ── Setters ──────────────────────────────────────────────────────────────────
void RuntimeConfig::setCenterLat(double v)          { s_centerLat = v; }
void RuntimeConfig::setCenterLon(double v)          { s_centerLon = v; }
void RuntimeConfig::setRadiusKm(double v)           { s_radiusKm = v; }
void RuntimeConfig::setFetchIntervalSec(uint32_t v) { s_fetchSec = v; }
void RuntimeConfig::setDisplayCycleSec(uint32_t v)  { s_cycleSec = v; }
void RuntimeConfig::setBrightness(uint8_t v)        { s_brightness = v; }
void RuntimeConfig::setOpenskyClientId(const String &v)     { s_openskyId = v; }
void RuntimeConfig::setOpenskyClientSecret(const String &v) { s_openskySecret = v; }
void RuntimeConfig::setAeroApiKey(const String &v)          { s_aeroApiKey = v; }
