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
    // calling every nearby callsign triggers HTTP 429. State vectors are already
    // ordered by distance so this naturally limits to the closest aircraft.
    static constexpr size_t MAX_AEROAPI_CALLS_PER_CYCLE = 5;
}
