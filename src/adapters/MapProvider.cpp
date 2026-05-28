#include "MapProvider.h"

#include <cmath>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>
#include "APIConfiguration.h"
#include "RuntimeConfig.h"
#include "debug.h"

static const char *MAP_PATH = "/mapcache.jpg";
static const char *NVS_NS   = "mapcache";

static uint32_t s_mapVersion = 0;

// ── Zoom calculation ──────────────────────────────────────────────────────────

int MapProvider::calcZoom(double radiusKm, double centerLat,
                          uint16_t screenW, uint16_t screenH)
{
    double cosLat = cos(centerLat * M_PI / 180.0);
    if (cosLat < 0.01) cosLat = 0.01;

    // Choose zoom so that 2.5× the radius (diameter + 25 % margin each side) fits
    // in the shorter usable display dimension (subtracting 20 px for the header bar).
    uint16_t minDim = (screenW < screenH) ? screenW : (uint16_t)(screenH > 20 ? screenH - 20 : screenH);
    double z = log2(minDim * 40075.0 * cosLat / (256.0 * 2.5 * radiusKm));
    int zi = (int)floor(z);
    if (zi < 8)  zi = 8;
    if (zi > 15) zi = 15;
    return zi;
}

// ── Cache validity ────────────────────────────────────────────────────────────

static bool isCacheValid(double cLat, double cLon, double rKm,
                         uint16_t screenW, uint16_t screenH)
{
    if (!LittleFS.exists(MAP_PATH)) return false;

    Preferences p;
    p.begin(NVS_NS, true);
    float    mc_lat  = p.getFloat("lat",  999.0f);
    float    mc_lon  = p.getFloat("lon",  999.0f);
    float    mc_rad  = p.getFloat("rad",  -1.0f);
    int      mc_zoom = p.getInt  ("zoom", -1);
    long     mc_time = p.getLong ("t",     0);
    uint16_t mc_w    = (uint16_t)p.getUInt("w", 0);
    uint16_t mc_h    = (uint16_t)p.getUInt("h", 0);
    int      mc_fmtv = p.getInt  ("fmtv", 0); // 0 = direct Google (progressive), 1 = weserv.nl (baseline)
    p.end();

    if (mc_fmtv != 1) return false; // format change: old cache used progressive JPEG, must re-fetch
    if (fabsf(mc_lat - (float)cLat) > 0.0001f) return false;
    if (fabsf(mc_lon - (float)cLon) > 0.0001f) return false;
    if (fabsf(mc_rad - (float)rKm)  > 0.1f)    return false;
    if (mc_zoom != MapProvider::calcZoom(rKm, cLat, screenW, screenH)) return false;
    if (mc_w != screenW || mc_h != screenH)     return false;

    long age = (long)time(nullptr) - mc_time;
    if (age < 0 || age > 86400) return false; // expired after 24 h

    return true;
}

// ── Public API ────────────────────────────────────────────────────────────────

const char *MapProvider::cachedMapPath() { return MAP_PATH; }
uint32_t    MapProvider::mapVersion()    { return s_mapVersion; }

bool MapProvider::ensureMapCached(uint16_t screenW, uint16_t screenH)
{
    double cLat = RuntimeConfig::centerLat();
    double cLon = RuntimeConfig::centerLon();
    double rKm  = RuntimeConfig::radiusKm();
    int    zoom = calcZoom(rKm, cLat, screenW, screenH);

    if (isCacheValid(cLat, cLon, rKm, screenW, screenH))
    {
        DBG_VERBOSE("MapProvider: cache valid (v=%u)", s_mapVersion);
        return true;
    }

    if (strlen(APIConfiguration::MAPS_API_KEY) == 0)
    {
        DBG_WARN("MapProvider: no API key — add SECRET_MAPS_API_KEY to secrets.h");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        DBG_WARN("MapProvider: WiFi not connected");
        return false;
    }

    // Route through images.weserv.nl which re-encodes the Google Maps Static API
    // progressive JPEG as baseline JPEG — TJpg_Decoder only supports baseline
    // (JDR_FMT3 "not supported JPEG standard" on progressive input).
    // The Google Maps URL is embedded as the `url=` parameter with & encoded as %26.
    const String gmapsPath = String("maps.googleapis.com/maps/api/staticmap")
        + "?center="  + String(cLat, 6) + "," + String(cLon, 6)
        + "%26zoom="  + zoom
        + "%26size="  + screenW + "x" + screenH
        + "%26maptype=roadmap"
        + "%26format=jpg"
        + "%26key="   + APIConfiguration::MAPS_API_KEY;
    String url = "https://images.weserv.nl/?url=" + gmapsPath + "&output=jpg";

    DBG_INFO("MapProvider: fetching %ux%u zoom=%d (via weserv.nl)", screenW, screenH, zoom);

    WiFiClientSecure client;
    client.setInsecure(); // Google root cert pinning can be added later
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(15000);

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        DBG_WARN("MapProvider: HTTP %d  wifi: %d  heap: %u", code, (int)WiFi.status(), ESP.getFreeHeap());
        http.end();
        return false;
    }

    // Stream response directly to LittleFS — avoids allocating the full image on heap
    File f = LittleFS.open(MAP_PATH, "w");
    if (!f)
    {
        DBG_ERROR("MapProvider: cannot open %s for write", MAP_PATH);
        http.end();
        return false;
    }

    int written = http.writeToStream(&f);
    f.close();
    http.end();

    if (written <= 0)
    {
        DBG_WARN("MapProvider: writeToStream returned %d", written);
        LittleFS.remove(MAP_PATH);
        return false;
    }

    // Confirm the first three bytes are a valid JPEG SOI marker (FF D8 FF).
    // writeToStream can silently write garbled data on ESP32-Arduino 3.x when
    // the underlying WiFiClientSecure stream is involved.
    {
        File chk = LittleFS.open(MAP_PATH, "r");
        uint8_t magic[3] = {0, 0, 0};
        if (chk) { chk.read(magic, 3); chk.close(); }
        if (magic[0] != 0xFF || magic[1] != 0xD8 || magic[2] != 0xFF)
        {
            DBG_WARN("MapProvider: JPEG magic invalid %02X %02X %02X — deleting corrupt cache",
                     magic[0], magic[1], magic[2]);
            LittleFS.remove(MAP_PATH);
            return false;
        }
    }

    // Persist cache metadata to NVS
    Preferences p;
    p.begin(NVS_NS, false);
    p.putFloat("lat",  (float)cLat);
    p.putFloat("lon",  (float)cLon);
    p.putFloat("rad",  (float)rKm);
    p.putInt  ("zoom", zoom);
    p.putLong ("t",    (long)time(nullptr));
    p.putUInt ("w",    screenW);
    p.putUInt ("h",    screenH);
    p.putInt  ("fmtv", 1); // 1 = weserv.nl baseline JPEG
    p.end();

    s_mapVersion++;
    DBG_INFO("MapProvider: cached %d bytes (v=%u zoom=%d)", written, s_mapVersion, zoom);
    return true;
}

bool MapProvider::latLonToPixel(double lat, double lon,
                                uint16_t screenW, uint16_t screenH,
                                int16_t &px, int16_t &py)
{
    if (isnan(lat) || isnan(lon)) return false;

    double cLat = RuntimeConfig::centerLat();
    double cLon = RuntimeConfig::centerLon();
    double rKm  = RuntimeConfig::radiusKm();
    int    zoom = calcZoom(rKm, cLat, screenW, screenH);

    // Total world width in pixels at this zoom (Web Mercator)
    double scale = 256.0 * (double)(1 << zoom);

    // Longitude is linear in Web Mercator
    double dx = (lon - cLon) * scale / 360.0;
    px = (int16_t)((double)(screenW / 2) + dx);

    // Latitude uses the Mercator formula
    double mLat = log(tan(M_PI / 4.0 + lat  * M_PI / 360.0));
    double mCtr = log(tan(M_PI / 4.0 + cLat * M_PI / 360.0));
    double dy   = -(mLat - mCtr) * scale / (2.0 * M_PI);
    py = (int16_t)((double)(screenH / 2) + dy);

    // Reject points outside the screen bounds
    if (px < 0 || px >= (int16_t)screenW) return false;
    if (py < 0 || py >= (int16_t)screenH) return false;
    return true;
}
