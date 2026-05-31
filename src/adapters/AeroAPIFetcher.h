#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "BaseFlightFetcher.h"
#include "APIConfiguration.h"

class AeroAPIFetcher : public BaseFlightFetcher
{
public:
    AeroAPIFetcher() = default;
    ~AeroAPIFetcher() override = default;

    bool fetchFlightInfo(const String &flightIdent, FlightInfo &outInfo) override;

    // Fetch live position (lat/lon/altitude/speed/heading) for an airborne flight
    // via GET /flights/search?query=-idents <ident>. The /flights/{ident} endpoint
    // (BaseFlight) carries no position; only the search endpoint (InFlightStatus)
    // returns last_position. Returns false if the flight is not airborne / no match.
    bool fetchLivePosition(const String &ident, FlightInfo &outInfo);
};
