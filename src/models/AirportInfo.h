#pragma once

#include <Arduino.h>

struct AirportInfo
{
    String code_icao;
    String code_iata;
    String city;       // e.g. "Sydney", "Melbourne" — from AeroAPI origin/destination.city
};
