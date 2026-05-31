#pragma once

#include <Arduino.h>
#include <vector>
#include <time.h>
#include "AirportInfo.h"

struct FlightInfo
{
    // True when the live state was matched to a current AeroAPI flight record.
    bool enriched = false;

    // True when this card was pinned by the user (nominated via WebUI config).
    bool pinned = false;

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
    String icao24;
    String origin_country;
    long time_position = 0;
    double lon = NAN;
    double lat = NAN;
    double distance_km = NAN;
    double bearing_deg = NAN;
    double baro_altitude_m = NAN;
    double geo_altitude_m = NAN;
    double velocity_mps = NAN;
    double heading_deg = NAN;
    double vertical_rate_mps = NAN;
    long last_contact = 0;
    String squawk;
    int position_source = 0;
    bool on_ground = false;

    // Human-friendly display strings
    String airline_display_name_full;
    String aircraft_display_name_short;

    // Airline brand color (RGB565). 0xFFFF = white fallback when CDN has no color.
    uint16_t airline_color = 0xFFFF;

    // Flight schedule timing (UTC epoch seconds). Zero = unknown.
    time_t actual_out_epoch  = 0; // actual departure gate push
    time_t estimated_in_epoch = 0; // estimated gate arrival

    // LittleFS path to cached airline logo JPEG, e.g. "/logos/QFA.jpg".
    // Empty = logo not available; display falls back to colored airline text.
    String logo_path;
};

// True if at least one flight carries a plottable position. The radar map card
// is only included in the display cycle when this holds — an empty map (e.g. a
// lone pinned placeholder with no fix) is useless and just doubles map dwell.
// CYDDisplay::displayFlights() and WebUIServer::onGetLive() MUST use this same
// helper so their slot maths stay in lockstep (see CLAUDE.md slot-tracking parity).
inline bool anyFlightLocatable(const std::vector<FlightInfo> &flights)
{
    for (const auto &f : flights)
        if (!isnan(f.lat) && !isnan(f.lon))
            return true;
    return false;
}
