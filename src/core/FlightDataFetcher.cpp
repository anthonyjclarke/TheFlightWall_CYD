/*
Purpose: Orchestrate fetching and enrichment of flight data for display.
Flow:
1) Use BaseStateVectorFetcher to fetch nearby state vectors by geo filter.
2) For each callsign, use BaseFlightFetcher (e.g., AeroAPI) to retrieve FlightInfo.
3) Enrich names via FlightWallFetcher (airline/aircraft display names).
Output: Returns count of enriched flights and fills outStates/outFlights.
*/
#include "FlightDataFetcher.h"
#include "RuntimeConfig.h"
#include "TimingConfiguration.h"
#include "FlightWallFetcher.h"
#include "debug.h"

FlightDataFetcher::FlightDataFetcher(BaseStateVectorFetcher *stateFetcher,
                                     BaseFlightFetcher *flightFetcher)
    : _stateFetcher(stateFetcher), _flightFetcher(flightFetcher) {}

size_t FlightDataFetcher::fetchFlights(std::vector<StateVector> &outStates,
                                       std::vector<FlightInfo> &outFlights)
{
    outStates.clear();
    outFlights.clear();

    bool ok = _stateFetcher->fetchStateVectors(
        RuntimeConfig::centerLat(),
        RuntimeConfig::centerLon(),
        RuntimeConfig::radiusKm(),
        outStates);
    if (!ok)
        return 0;

    size_t enriched = 0;
    size_t aeroCallCount = 0;

    for (const StateVector &s : outStates)
    {
        // Trim trailing spaces (OpenSky pads callsigns to 8 chars).
        String callsign = s.callsign;
        callsign.trim();

        FlightInfo info;
        info.ident = callsign.length() ? callsign : s.icao24;

        // The ADS-B card is displayable even when AeroAPI enrichment is unavailable.
        info.icao24            = s.icao24;
        info.origin_country    = s.origin_country;
        info.time_position     = s.time_position;
        info.lon               = s.lon;
        info.lat               = s.lat;
        info.distance_km       = s.distance_km;
        info.bearing_deg       = s.bearing_deg;
        info.baro_altitude_m   = s.baro_altitude;
        info.geo_altitude_m    = s.geo_altitude;
        info.velocity_mps      = s.velocity;
        info.heading_deg       = s.heading;
        info.vertical_rate_mps = s.vertical_rate;
        info.last_contact      = s.last_contact;
        info.squawk            = s.squawk;
        info.position_source   = s.position_source;
        info.on_ground         = s.on_ground;

        // A valid ICAO flight-number callsign is 3-letter airline prefix + digits.
        // Reject: too short, no digit, embedded space, or < 3 leading alpha chars
        // ("VV922" fails the prefix check; "DELTA" fails no-digit; "RED O" fails space).
        bool hasDigit = false;
        bool hasEmbeddedSpace = false;
        size_t alphaPrefix = 0;
        for (size_t i = 0; i < callsign.length(); i++)
        {
            const char c = callsign[i];
            if (isdigit((unsigned char)c))            hasDigit = true;
            if (c == ' ')                             hasEmbeddedSpace = true;
            if (!hasDigit && isalpha((unsigned char)c)) alphaPrefix++;
        }
        const bool canEnrich = callsign.length() >= 3 &&
                               hasDigit && !hasEmbeddedSpace && alphaPrefix >= 3;
        if (!canEnrich)
        {
            DBG_VERBOSE("Display '%s' without AeroAPI enrichment: invalid flight callsign",
                        info.ident.c_str());
            outFlights.push_back(info);
            continue;
        }

        if (aeroCallCount < TimingConfiguration::MAX_AEROAPI_CALLS_PER_CYCLE)
        {
            aeroCallCount++;
            if (_flightFetcher->fetchFlightInfo(callsign, info))
            {
                info.enriched = true;
                FlightWallFetcher fw;
                if (info.operator_icao.length())
                {
                    String airlineFull;
                    uint16_t airlineColor;
                    if (fw.getAirlineData(info.operator_icao, airlineFull, airlineColor))
                    {
                        info.airline_display_name_full = airlineFull;
                        info.airline_color = airlineColor;
                    }

                    String logoPath;
                    if (fw.getAirlineLogo(info.operator_icao, info.operator_iata, logoPath))
                        info.logo_path = logoPath;
                }
                if (info.aircraft_code.length())
                {
                    String aircraftShort, aircraftFull;
                    if (fw.getAircraftName(info.aircraft_code, aircraftShort, aircraftFull))
                    {
                        if (aircraftShort.length())
                        {
                            info.aircraft_display_name_short = aircraftShort;
                        }
                    }
                }
                enriched++;
            }
        }
        else
        {
            DBG_VERBOSE("Display '%s' without AeroAPI enrichment: cycle API limit reached",
                        callsign.c_str());
        }

        outFlights.push_back(info);
    }
    DBG_INFO("FlightData: states=%u cards=%u aero_calls=%u enriched=%u",
             (unsigned)outStates.size(),
             (unsigned)outFlights.size(),
             (unsigned)aeroCallCount,
             (unsigned)enriched);
    return enriched;
}
