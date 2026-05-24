#pragma once

#include <Arduino.h>

namespace TimingConfiguration
{
    // OpenSky free tier ≈ 4 000 requests/month — 30s gives ~86 000/month at peak
    static constexpr uint32_t FETCH_INTERVAL_SECONDS = 30;

    // How long each flight card stays on screen before cycling to the next
    static constexpr uint32_t DISPLAY_CYCLE_SECONDS = 3;
}
