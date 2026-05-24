#pragma once

#include <Arduino.h>
#include <vector>
#include "AirportInfo.h"

struct FlightInfo
{
    // Flight identifiers
    String ident;
    String ident_icao;
    String ident_iata;

    // Operator
    String operator_code;
    String operator_icao;
    String operator_iata;

    // Route
    AirportInfo origin;
    AirportInfo destination;

    // Aircraft
    String aircraft_code;

    // Live ADS-B state from OpenSky. These fields come from the state-vector
    // query already being made and do not require additional API calls.
    String origin_country;
    double distance_km = NAN;
    double bearing_deg = NAN;
    double baro_altitude_m = NAN;
    double geo_altitude_m = NAN;
    double velocity_mps = NAN;
    double heading_deg = NAN;
    double vertical_rate_mps = NAN;
    long last_contact = 0;
    bool on_ground = false;

    // Human-friendly display strings
    String airline_display_name_full;
    String aircraft_display_name_short;
};
