/*
Purpose: Retrieve detailed flight metadata from AeroAPI over HTTPS.
Responsibilities:
- Perform authenticated GET to /flights/{ident} using API key.
- Parse minimal fields into FlightInfo (ident/operator/aircraft and ICAO codes).
- Handle TLS (optionally insecure for dev) and JSON errors gracefully.
Input: flight ident (e.g., callsign).
Output: Populates FlightInfo on success and returns true.
*/
#include "AeroAPIFetcher.h"
#include "RuntimeConfig.h"
#include "debug.h"

static String safeGetString(JsonVariant v, const char *key)
{
    if (!v.containsKey(key) || v[key].isNull())
        return String("");
    return String(v[key].as<const char *>());
}

static time_t utcCalendarToEpoch(const struct tm &t)
{
    int year = t.tm_year + 1900;
    const int month = t.tm_mon + 1;
    const int day = t.tm_mday;
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const int yearOfEra = year - era * 400;
    const int dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    const int daysSinceEpoch = era * 146097 + dayOfEra - 719468;
    return (time_t)daysSinceEpoch * 86400L +
           (time_t)t.tm_hour * 3600L + (time_t)t.tm_min * 60L + t.tm_sec;
}

// Parses ISO 8601 timestamps (with or without timezone offset) → UTC epoch.
// Handles "2026-05-25T08:00:00+08:00", "...Z", and "...T08:00:00" (no offset).
static time_t parseIso8601(const char *s)
{
    if (!s || !*s) return 0;
    struct tm t = {};
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d",
               &t.tm_year, &t.tm_mon, &t.tm_mday,
               &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
        return 0;
    t.tm_year -= 1900;
    t.tm_mon  -= 1;
    t.tm_isdst = 0;
    // Advance past optional fractional seconds, then read timezone designator.
    const char *p = s + 19;
    if (*p == '.') { while (*p && *p != '+' && *p != '-' && *p != 'Z') p++; }
    int tzOffsetSec = 0;
    if (*p == '+' || *p == '-')
    {
        int tzH = 0, tzM = 0;
        sscanf(p + 1, "%2d:%2d", &tzH, &tzM);
        tzOffsetSec = (tzH * 3600 + tzM * 60) * ((*p == '+') ? 1 : -1);
    }
    // The parser must remain UTC even though serial diagnostics use local time.
    return utcCalendarToEpoch(t) - tzOffsetSec;
}

static time_t flightDepartureEpoch(JsonObject flight)
{
    const char *timestamp = !flight["actual_out"].isNull()
                                ? flight["actual_out"].as<const char *>()
                                : (!flight["scheduled_out"].isNull()
                                       ? flight["scheduled_out"].as<const char *>()
                                       : nullptr);
    return parseIso8601(timestamp);
}

static time_t flightArrivalEpoch(JsonObject flight)
{
    const char *timestamp = !flight["actual_in"].isNull()
                                ? flight["actual_in"].as<const char *>()
                                : (!flight["estimated_in"].isNull()
                                       ? flight["estimated_in"].as<const char *>()
                                       : (!flight["scheduled_in"].isNull()
                                              ? flight["scheduled_in"].as<const char *>()
                                              : nullptr));
    return parseIso8601(timestamp);
}

bool AeroAPIFetcher::fetchFlightInfo(const String &flightIdent, FlightInfo &outInfo)
{
    const String apiKey = RuntimeConfig::aeroApiKey();
    if (apiKey.length() == 0)
    {
        DBG_WARN("AeroAPI: no API key configured");
        return false;
    }

    WiFiClientSecure client;
    if (APIConfiguration::AEROAPI_INSECURE_TLS)
    {
        client.setInsecure();
    }

    HTTPClient http;
    String url = String(APIConfiguration::AEROAPI_BASE_URL) + "/flights/" + flightIdent;
    http.begin(client, url);
    http.setTimeout(15000);
    http.addHeader("x-apikey", apiKey);
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != 200)
    {
        DBG_WARN("AeroAPI: HTTP %d (%s) for %s",
                 code,
                 HTTPClient::errorToString(code).c_str(),
                 flightIdent.c_str());
        http.end();
        return false;
    }

    // Read entire body into a String before parsing.
    // getStream() on ESP32-Arduino-3.x with WiFiClientSecure sometimes delivers
    // zero bytes to the ArduinoJson parser (doc-mem=0, err=Ok) due to SSL
    // buffering behaviour. getString() buffers the full body first, avoiding this.
    // With doc=16 384 and typical AeroAPI responses ≤30 KB the combined heap
    // usage is well within the 200 KB+ free heap on this device.
    String body = http.getString();
    http.end();
    const int payloadLen = body.length();

    DBG_VERBOSE("AeroAPI body[%.80s]", body.c_str());

    StaticJsonDocument<768> filter;
    filter["flights"][0]["ident"] = true;
    filter["flights"][0]["ident_icao"] = true;
    filter["flights"][0]["ident_iata"] = true;
    filter["flights"][0]["operator"] = true;
    filter["flights"][0]["operator_icao"] = true;
    filter["flights"][0]["operator_iata"] = true;
    filter["flights"][0]["aircraft_type"] = true;
    filter["flights"][0]["origin"]["code_icao"] = true;
    filter["flights"][0]["origin"]["code_iata"] = true;
    filter["flights"][0]["origin"]["city"] = true;
    filter["flights"][0]["destination"]["code_icao"] = true;
    filter["flights"][0]["destination"]["code_iata"] = true;
    filter["flights"][0]["destination"]["city"] = true;
    filter["flights"][0]["actual_out"] = true;
    filter["flights"][0]["scheduled_out"] = true;
    filter["flights"][0]["actual_in"] = true;
    filter["flights"][0]["estimated_in"] = true;
    filter["flights"][0]["scheduled_in"] = true;

    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, body,
                                               DeserializationOption::Filter(filter));

    if (err)
    {
        DBG_ERROR("AeroAPI: JSON parse failed for %s: %s  payload_len=%d",
                  flightIdent.c_str(), err.c_str(), payloadLen);
        return false;
    }

    JsonArray flights = doc["flights"].as<JsonArray>();
    if (flights.isNull() || flights.size() == 0)
    {
        DBG_WARN("AeroAPI: no flights in response for %s  body=%d bytes  doc-mem=%u",
                 flightIdent.c_str(), payloadLen, doc.memoryUsage());
        return false;
    }

    const time_t nowSel = time(nullptr);
    if (nowSel <= 1000000000L)
    {
        DBG_WARN("AeroAPI: cannot select active record for %s before time sync",
                 flightIdent.c_str());
        return false;
    }

    // A callsign can be reused and AeroAPI returns historical legs. Select only
    // a leg plausibly associated with an aircraft detected live by OpenSky now.
    static constexpr time_t ARRIVAL_GRACE_SECONDS = 30 * 60;
    static constexpr time_t MAX_UNKNOWN_ARRIVAL_AGE_SECONDS = 20 * 60 * 60;
    int bestIdx = -1;
    int bestRank = 0;
    time_t bestDep = 0;
    time_t bestArr = 0;
    time_t newestPastDep = 0;
    time_t newestPastArr = 0;

    for (int i = 0; i < (int)flights.size(); i++)
    {
        JsonObject candidate = flights[i].as<JsonObject>();
        const time_t dep = flightDepartureEpoch(candidate);
        if (dep <= 0 || dep > nowSel)
            continue;

        const time_t arr = flightArrivalEpoch(candidate);
        if (dep > newestPastDep)
        {
            newestPastDep = dep;
            newestPastArr = arr;
        }

        const bool notYetArrived = arr > 0 && nowSel <= arr;
        const bool justArrived = arr > 0 && nowSel <= arr + ARRIVAL_GRACE_SECONDS;
        const bool recentWithoutArrival =
            arr == 0 && nowSel - dep <= MAX_UNKNOWN_ARRIVAL_AGE_SECONDS;
        const int rank = notYetArrived ? 2 : ((justArrived || recentWithoutArrival) ? 1 : 0);
        if (rank > bestRank || (rank == bestRank && rank > 0 && dep > bestDep))
        {
            bestRank = rank;
            bestIdx = i;
            bestDep = dep;
            bestArr = arr;
        }
    }

    if (bestIdx < 0)
    {
        DBG_WARN("AeroAPI: %s no active match in %d records; newest departure age=%ldmin arrival_delta=%ldmin",
                 flightIdent.c_str(), (int)flights.size(),
                 newestPastDep > 0 ? (long)(nowSel - newestPastDep) / 60 : -1L,
                 newestPastArr > 0 ? (long)(newestPastArr - nowSel) / 60 : -1L);
        return false;
    }

    DBG_INFO("AeroAPI: %s - %d records, selected idx=%d (dep_delta=%ldmin arr_delta=%ldmin)",
             flightIdent.c_str(), (int)flights.size(), bestIdx,
             (long)(bestDep - nowSel) / 60,
             bestArr > 0 ? (long)(bestArr - nowSel) / 60 : -1L);

    JsonObject f = flights[bestIdx].as<JsonObject>();
    outInfo.ident = safeGetString(f, "ident");
    outInfo.ident_icao = safeGetString(f, "ident_icao");
    outInfo.ident_iata = safeGetString(f, "ident_iata");
    outInfo.operator_code = safeGetString(f, "operator");
    outInfo.operator_icao = safeGetString(f, "operator_icao");
    outInfo.operator_iata = safeGetString(f, "operator_iata");
    outInfo.aircraft_code = safeGetString(f, "aircraft_type");

    if (f.containsKey("origin") && f["origin"].is<JsonObject>())
    {
        JsonObject o = f["origin"].as<JsonObject>();
        outInfo.origin.code_icao = safeGetString(o, "code_icao");
        outInfo.origin.code_iata = safeGetString(o, "code_iata");
        outInfo.origin.city      = safeGetString(o, "city");
    }

    if (f.containsKey("destination") && f["destination"].is<JsonObject>())
    {
        JsonObject d = f["destination"].as<JsonObject>();
        outInfo.destination.code_icao = safeGetString(d, "code_icao");
        outInfo.destination.code_iata = safeGetString(d, "code_iata");
        outInfo.destination.city      = safeGetString(d, "city");
    }

    // Departure: prefer actual_out, fall back to scheduled_out
    const char *depStr = (!f["actual_out"].isNull())
                         ? f["actual_out"].as<const char *>()
                         : (!f["scheduled_out"].isNull() ? f["scheduled_out"].as<const char *>() : nullptr);
    if (depStr)
    {
        outInfo.actual_out_epoch = parseIso8601(depStr);
        DBG_INFO("AeroAPI: %s dep=\"%s\" epoch=%lu now=%lu delta=%ldmin",
                 flightIdent.c_str(), depStr,
                 (unsigned long)outInfo.actual_out_epoch,
                 (unsigned long)nowSel,
                 (long)(outInfo.actual_out_epoch - nowSel) / 60);
    }

    // Arrival: prefer actual_in, then estimated_in, then scheduled_in
    const char *arrStr = (!f["actual_in"].isNull())
                         ? f["actual_in"].as<const char *>()
                         : (!f["estimated_in"].isNull()
                         ? f["estimated_in"].as<const char *>()
                         : (!f["scheduled_in"].isNull() ? f["scheduled_in"].as<const char *>() : nullptr));
    if (arrStr)
    {
        outInfo.estimated_in_epoch = parseIso8601(arrStr);
        DBG_INFO("AeroAPI: %s arr=\"%s\" epoch=%lu delta=%ldmin",
                 flightIdent.c_str(), arrStr,
                 (unsigned long)outInfo.estimated_in_epoch,
                 (long)(outInfo.estimated_in_epoch - nowSel) / 60);
    }

    // NOTE: /flights/{ident} (BaseFlight schema) carries no position data. Live
    // lat/lon comes from fetchLivePosition() via /flights/search (InFlightStatus).
    return true;
}

bool AeroAPIFetcher::fetchLivePosition(const String &ident, FlightInfo &outInfo)
{
    const String apiKey = RuntimeConfig::aeroApiKey();
    if (apiKey.length() == 0)
    {
        DBG_WARN("AeroAPI: no API key for position search");
        return false;
    }

    WiFiClientSecure client;
    if (APIConfiguration::AEROAPI_INSECURE_TLS)
        client.setInsecure();

    HTTPClient http;
    // /flights/search returns InFlightStatus (airborne flights only), which is the
    // only endpoint that carries last_position. Query operator: "-idents <IDENT>";
    // the space is URL-encoded and idents are plain alphanumerics.
    String url = String(APIConfiguration::AEROAPI_BASE_URL) +
                 "/flights/search?query=-idents%20" + ident;
    http.begin(client, url);
    http.setTimeout(15000);
    http.addHeader("x-apikey", apiKey);
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != 200)
    {
        DBG_WARN("AeroAPI: position search HTTP %d (%s) for %s",
                 code, HTTPClient::errorToString(code).c_str(), ident.c_str());
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();
    DBG_VERBOSE("AeroAPI search body[%.80s]", body.c_str());

    StaticJsonDocument<256> filter;
    filter["flights"][0]["last_position"]["latitude"]    = true;
    filter["flights"][0]["last_position"]["longitude"]   = true;
    filter["flights"][0]["last_position"]["altitude"]    = true;
    filter["flights"][0]["last_position"]["groundspeed"] = true;
    filter["flights"][0]["last_position"]["heading"]     = true;

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err)
    {
        DBG_ERROR("AeroAPI: position search parse error: %s", err.c_str());
        return false;
    }

    JsonArray flights = doc["flights"].as<JsonArray>();
    if (flights.isNull() || flights.size() == 0)
    {
        DBG_INFO("AeroAPI: %s not airborne (no /search match)", ident.c_str());
        return false;
    }
    JsonObject p = flights[0]["last_position"].as<JsonObject>();
    if (p.isNull() || p["latitude"].isNull() || p["longitude"].isNull())
    {
        DBG_INFO("AeroAPI: %s /search match but no last_position", ident.c_str());
        return false;
    }

    outInfo.lat = p["latitude"].as<double>();
    outInfo.lon = p["longitude"].as<double>();
    if (!p["altitude"].isNull())
        outInfo.baro_altitude_m = p["altitude"].as<double>() * 100.0 / 3.28084; // 100s ft → m
    if (!p["groundspeed"].isNull())
        outInfo.velocity_mps = p["groundspeed"].as<double>() * 0.514444; // kt → m/s
    if (!p["heading"].isNull())
        outInfo.heading_deg = p["heading"].as<double>();
    DBG_INFO("AeroAPI: %s live position %.4f,%.4f", ident.c_str(), outInfo.lat, outInfo.lon);
    return true;
}
