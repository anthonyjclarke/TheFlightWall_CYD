#pragma once

#include <Arduino.h>

namespace UserConfiguration
{
    // Geographic centre for nearby-flight search.
    // These will be configurable via the WebUI in a future phase.
    static constexpr double CENTER_LAT = -33.8688; // Sydney, AU (example)
    static constexpr double CENTER_LON = 151.2093;
    static constexpr double RADIUS_KM  = 50.0;

    // Display colour palette (RGB565).
    // Background is always black; these control text and accent colours.
    // 0xFD20 = orange/amber — good aviation-style accent
    static constexpr uint16_t COLOR_BACKGROUND = 0x0000; // black
    static constexpr uint16_t COLOR_HEADER_BG  = 0x0842; // dark navy
    static constexpr uint16_t COLOR_AIRLINE     = 0xFFFF; // white
    static constexpr uint16_t COLOR_ROUTE       = 0xFD20; // amber
    static constexpr uint16_t COLOR_SUB         = 0xC618; // light grey
    static constexpr uint16_t COLOR_DIVIDER     = 0x4208; // dark grey
    static constexpr uint16_t COLOR_MESSAGE     = 0xFFFF; // white
}
