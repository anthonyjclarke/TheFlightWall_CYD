#pragma once

#include <Arduino.h>
#include <vector>
#include "models/FlightInfo.h"

class BaseDisplay
{
public:
    virtual ~BaseDisplay() = default;
    virtual bool initialize()                                    = 0;
    virtual void clear()                                         = 0;
    virtual void displayFlights(const std::vector<FlightInfo> &) = 0;
    virtual void displayMessage(const String &message)          = 0;
    virtual void showLoading()                                   = 0;
};
