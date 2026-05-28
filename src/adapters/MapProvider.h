#pragma once

#include <stdint.h>

// MapProvider — fetches and caches a Google Maps Static API background image for the
// map card, and handles Web Mercator lat/lon → screen-pixel conversion.
//
// The map is stored as /mapcache.jpg on LittleFS (single file, not tiles).
// Cache metadata (center, radius, zoom, timestamp) lives in the NVS namespace "mapcache".
// The cache is valid for 24 hours; it is also invalidated when the configured center or
// radius changes.
//
// ensureMapCached() should be called during the periodic flight-fetch cycle so the
// map is always ready before the map card is displayed.  It returns quickly (no network
// call) when the cache is still valid.

namespace MapProvider
{
    // Download and cache a static map image sized screenW × screenH.
    // Returns true if a valid cached image is available (after fetching if needed).
    // Returns false if no API key is configured, WiFi is unavailable, or the fetch fails.
    bool ensureMapCached(uint16_t screenW, uint16_t screenH);

    // Absolute LittleFS path of the cached map JPEG.
    const char *cachedMapPath();

    // Convert a WGS84 lat/lon to a screen pixel using the same Web Mercator projection
    // that Google Maps Static API uses for the cached image.
    // Returns false if lat or lon is NaN or if the result lies outside the screen.
    bool latLonToPixel(double lat, double lon,
                       uint16_t screenW, uint16_t screenH,
                       int16_t &px, int16_t &py);

    // Select an appropriate zoom level so the configured radius fits comfortably on screen.
    int calcZoom(double radiusKm, double centerLat, uint16_t screenW, uint16_t screenH);

    // Monotonically-incrementing counter — bumped each time a new map is downloaded.
    // Used by CYDDisplay::mapRenderKey() to force a redraw after a cache refresh.
    uint32_t mapVersion();
}
