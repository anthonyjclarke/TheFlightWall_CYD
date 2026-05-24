#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "APIConfiguration.h"

class FlightWallFetcher
{
public:
    FlightWallFetcher() = default;
    ~FlightWallFetcher() = default;

    // Fetches display name and brand color in a single HTTP call.
    // outColorRgb565 is 0xFFFF (white) if CDN has no brand_color_hex field.
    bool getAirlineData(const String &airlineIcao,
                        String &outDisplayNameFull,
                        uint16_t &outColorRgb565);

    bool getAircraftName(const String &aircraftIcao,
                         String &outDisplayNameShort,
                         String &outDisplayNameFull);

    // Downloads the airline logo JPEG to LittleFS if not already cached.
    // Tries airlineIcao first; falls back to airlineIata if the ICAO download fails.
    // Returns true and sets outLfsPath on success.
    bool getAirlineLogo(const String &airlineIcao, const String &airlineIata, String &outLfsPath);

private:
    bool httpGetJson(const String &url, String &outPayload);
    bool httpGetToFile(const String &url, const String &lfsPath);
};
