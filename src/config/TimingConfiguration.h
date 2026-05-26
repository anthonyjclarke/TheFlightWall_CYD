#pragma once

#include <Arduino.h>

namespace TimingConfiguration
{
    // POSIX timezone for local serial timestamps; includes Sydney DST changes.
    static constexpr const char *LOCAL_TIMEZONE = "AEST-10AEDT,M10.1.0,M4.1.0/3";

    // OpenSky free tier ≈ 4 000 requests/month — 30s gives ~86 000/month at peak
    static constexpr uint32_t FETCH_INTERVAL_SECONDS = 30;

    // How long each flight card stays on screen before cycling to the next
    static constexpr uint32_t DISPLAY_CYCLE_SECONDS = 3;

    // Maximum AeroAPI calls per fetch cycle. AeroAPI free tier enforces ~1 req/s;
    // each call takes ~10–15 s in practice so 10 calls adds ~100–150 s to a fetch.
    // State vectors are ordered by distance so this caps to the closest aircraft.
    // Raise this if you regularly see more flights than this value within radius.
    static constexpr size_t MAX_AEROAPI_CALLS_PER_CYCLE = 10;
}
