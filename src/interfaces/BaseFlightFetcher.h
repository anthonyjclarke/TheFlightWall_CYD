#pragma once

#include <Arduino.h>
#include "FlightInfo.h"

class BaseFlightFetcher
{
public:
    virtual ~BaseFlightFetcher() = default;
    virtual bool fetchFlightInfo(const String &flightIdent, FlightInfo &outInfo) = 0;
};
