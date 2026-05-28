#pragma once

#include <Arduino.h>

namespace TimingConfiguration
{
    // POSIX timezone for local serial timestamps; includes Sydney DST changes.
    static constexpr const char *LOCAL_TIMEZONE = "AEST-10AEDT,M10.1.0,M4.1.0/3";

    // OpenSky standard tier = 4 000 credits/day. 30s interval = 2 880 fetches/day at
    // 1 credit each (bounding box ≤ 25 sq°). Boxes > ~280 km radius cost 2 credits/fetch
    // (5 760/day) and will exhaust the quota before midnight.
    static constexpr uint32_t FETCH_INTERVAL_SECONDS = 30;

    // How long each flight card stays on screen before cycling to the next
    static constexpr uint32_t DISPLAY_CYCLE_SECONDS = 3;

    // How long the map card stays on screen (longer than flight cards for context)
    static constexpr uint32_t DISPLAY_MAP_SECONDS = 15;

    // Grace window after boot before the first blocking API fetch fires.
    // Gives the WebUI time to accept its first connection before OpenSky / AeroAPI
    // calls make loop() unresponsive for up to ~30 s.
    static constexpr uint32_t STARTUP_WEBUI_GRACE_MS = 8000;

    // Maximum AeroAPI calls per fetch cycle. AeroAPI free tier enforces ~1 req/s;
    // each call takes ~10–15 s in practice so 10 calls adds ~100–150 s to a fetch.
    // State vectors are ordered by distance so this caps to the closest aircraft.
    // Raise this if you regularly see more flights than this value within radius.
    static constexpr size_t MAX_AEROAPI_CALLS_PER_CYCLE = 10;
}
