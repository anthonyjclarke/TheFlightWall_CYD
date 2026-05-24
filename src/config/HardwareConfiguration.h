#pragma once

#include <Arduino.h>

namespace HardwareConfiguration
{
    // TFT backlight PWM — pin is set via build flag TFT_BL
    static constexpr uint8_t  BL_PWM_CHANNEL = 0;
    static constexpr uint32_t BL_PWM_FREQ    = 5000;
    static constexpr uint8_t  BL_PWM_BITS    = 8;
    static constexpr uint8_t  BL_BRIGHTNESS  = 200; // 0–255

    // WiFiManager captive-portal AP name (first-boot provisioning)
    static constexpr const char *WIFI_AP_NAME = "FlightWall-Setup";
}
