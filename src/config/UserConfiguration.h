#pragma once

#include <Arduino.h>

namespace UserConfiguration
{
    // Geographic centre for nearby-flight search.
    // These will be configurable via the WebUI in a future phase.
    static constexpr double CENTER_LAT = -33.823358; // Sydney, AU (example)
    static constexpr double CENTER_LON = 151.108;
    static constexpr double RADIUS_KM  = 10.0;

    // Display colour palette (RGB565).
    // Background is always black; these control text and accent colours.
    // 0xFD20 = orange/amber — good aviation-style accent
    static constexpr uint16_t COLOR_BACKGROUND  = 0x0000; // black
    static constexpr uint16_t COLOR_HEADER_BG   = 0x0842; // dark navy (loading/message screens)
    static constexpr uint16_t COLOR_AIRLINE      = 0xFFFF; // white (fallback for airline name)
    static constexpr uint16_t COLOR_ROUTE        = 0xFD20; // amber — IATA route
    static constexpr uint16_t COLOR_CALLSIGN     = 0xFFFF; // white — flight number
    static constexpr uint16_t COLOR_SUB          = 0xC618; // light grey — aircraft, status text
    static constexpr uint16_t COLOR_DIVIDER      = 0x2104; // very dark grey
    static constexpr uint16_t COLOR_MESSAGE      = 0xFFFF; // white
    static constexpr uint16_t COLOR_PROGRESS     = 0x07E0; // green — filled portion of bar
    static constexpr uint16_t COLOR_PROGRESS_BG  = 0x01C0; // dark green — unfilled portion
}
