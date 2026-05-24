/*
Purpose: Orchestrate fetching and enrichment of flight data for display.
Flow:
1) Use BaseStateVectorFetcher to fetch nearby state vectors by geo filter.
2) For each callsign, use BaseFlightFetcher (e.g., AeroAPI) to retrieve FlightInfo.
3) Enrich names via FlightWallFetcher (airline/aircraft display names).
Output: Returns count of enriched flights and fills outStates/outFlights.
*/
#include "core/FlightDataFetcher.h"
#include "config/RuntimeConfig.h"
#include "adapters/FlightWallFetcher.h"

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
    for (const StateVector &s : outStates)
    {
        if (s.callsign.length() == 0)
        {
            continue;
        }
        FlightInfo info;
        if (_flightFetcher->fetchFlightInfo(s.callsign, info))
        {
            info.origin_country    = s.origin_country;
            info.distance_km       = s.distance_km;
            info.bearing_deg       = s.bearing_deg;
            info.baro_altitude_m   = s.baro_altitude;
            info.geo_altitude_m    = s.geo_altitude;
            info.velocity_mps      = s.velocity;
            info.heading_deg       = s.heading;
            info.vertical_rate_mps = s.vertical_rate;
            info.last_contact      = s.last_contact;
            info.on_ground         = s.on_ground;

            FlightWallFetcher fw;
            if (info.operator_icao.length())
            {
                String airlineFull;
                if (fw.getAirlineName(info.operator_icao, airlineFull))
                {
                    info.airline_display_name_full = airlineFull;
                }
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
            outFlights.push_back(info);
            enriched++;
        }
    }
    return enriched;
}
