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

// A card is worth showing on the TFT / dashboard if at least one is true:
//  - AeroAPI confirmed an active flight record (route + timing known)
//  - The aircraft is airborne and has a real callsign (hex-only idents and
//    on-ground unenriched targets are uninformative empty cards)
static bool isDisplayableCard(const FlightInfo &f)
{
    if (f.enriched)          return true;
    if (f.ident == f.icao24) return false; // no callsign — just transponder hex
    if (f.on_ground)         return false; // parked/taxiing with no AeroAPI match
    return true;
}

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
            // 1–2 letter prefixes (e.g. PE771) are government/charter/military —
            // no AeroAPI record exists and the card would show --- - --- with no route.
            if (alphaPrefix >= 1 && alphaPrefix < 3)
            {
                DBG_VERBOSE("Skip '%s': non-standard ICAO prefix (%u chars), unenrichable",
                            info.ident.c_str(), (unsigned)alphaPrefix);
                continue;
            }
            // Pure-alpha callsigns (CHK, LIFR, etc.) have no flight number and are
            // helicopter/charter/government traffic — AeroAPI has no record for them.
            if (!hasDigit && alphaPrefix >= 3)
            {
                DBG_VERBOSE("Skip '%s': pure-alpha callsign, not a scheduled flight number",
                            info.ident.c_str());
                continue;
            }
            DBG_VERBOSE("ADS-B-only '%s': invalid flight callsign", info.ident.c_str());
            if (isDisplayableCard(info))
                outFlights.push_back(info);
            else
                DBG_VERBOSE("Skip '%s': no callsign or on-ground without enrichment",
                            info.ident.c_str());
            continue;
        }

        // ATC appends a letter suffix for duplicate departures (QLK423D → QLK423).
        // AeroAPI indexes by the base flight number, not the ATC suffix, so strip it.
        String aeroIdent = callsign;
        if (aeroIdent.length() > 4 &&
            isalpha((unsigned char)aeroIdent[aeroIdent.length() - 1]) &&
            isdigit((unsigned char)aeroIdent[aeroIdent.length() - 2]))
        {
            aeroIdent.remove(aeroIdent.length() - 1);
            DBG_VERBOSE("AeroAPI: stripped suffix '%s' -> '%s'",
                        callsign.c_str(), aeroIdent.c_str());
        }

        if (aeroCallCount < TimingConfiguration::MAX_AEROAPI_CALLS_PER_CYCLE)
        {
            aeroCallCount++;
            if (_flightFetcher->fetchFlightInfo(aeroIdent, info))
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
            else
            {
                // AeroAPI was called and explicitly returned no active record.
                // The flight is likely in transition (just rotated / on short final).
                // Suppress rather than show an "Airborne" placeholder with no data.
                DBG_VERBOSE("Skip '%s': AeroAPI found no active record", callsign.c_str());
                continue;
            }
        }
        else
        {
            // Per-cycle AeroAPI call limit reached — suppress rather than show a
            // placeholder with no route. Visible in WebUI activity feed via raw states.
            DBG_VERBOSE("Skip '%s': AeroAPI call limit reached this cycle", callsign.c_str());
            continue;
        }

        if (isDisplayableCard(info))
            outFlights.push_back(info);
        else
            DBG_VERBOSE("Skip '%s': on_ground=%d enriched=%d ident==icao24=%d",
                        info.ident.c_str(), info.on_ground, info.enriched,
                        info.ident == info.icao24);
    }
    DBG_INFO("FlightData: states=%u cards=%u aero_calls=%u enriched=%u",
             (unsigned)outStates.size(),
             (unsigned)outFlights.size(),
             (unsigned)aeroCallCount,
             (unsigned)enriched);
    return enriched;
}
