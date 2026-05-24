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

// Parses ISO 8601 timestamps (with or without timezone offset) → UTC epoch.
// Handles "2026-05-25T08:00:00+08:00", "...Z", and "...T08:00:00" (no offset).
// Requires the system clock to be set to UTC (via configTime(0,0,...)).
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
    // mktime treats struct tm as local time; with configTime(0,0,...) local=UTC.
    return mktime(&t) - tzOffsetSec;
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

    // AeroAPI returns flights sorted by scheduled_out descending (newest first).
    // flights[0] may be a future scheduled departure rather than the airborne flight.
    // Select the entry with the most recent departure that is still in the past.
    // Falls back to index 0 when NTP is unavailable or all entries are future.
    int bestIdx = 0;
    const time_t nowSel = time(nullptr);
    if (nowSel > 1000000000L && flights.size() > 1)
    {
        time_t bestDep = 0;
        for (int i = 0; i < (int)flights.size(); i++)
        {
            JsonObject fi = flights[i].as<JsonObject>();
            const char *ds = !fi["actual_out"].isNull()   ? fi["actual_out"].as<const char *>()
                           : !fi["scheduled_out"].isNull() ? fi["scheduled_out"].as<const char *>()
                           : nullptr;
            if (!ds) continue;
            time_t dep = parseIso8601(ds);
            if (dep > 0 && dep <= nowSel && dep > bestDep)
            {
                bestDep = dep;
                bestIdx = i;
            }
        }
        DBG_INFO("AeroAPI: %s — %d records, selected idx=%d (dep_epoch=%lu)",
                 flightIdent.c_str(), (int)flights.size(), bestIdx, (unsigned long)bestDep);
    }

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

    // Arrival: prefer estimated_in, fall back to scheduled_in
    const char *arrStr = (!f["estimated_in"].isNull())
                         ? f["estimated_in"].as<const char *>()
                         : (!f["scheduled_in"].isNull() ? f["scheduled_in"].as<const char *>() : nullptr);
    if (arrStr)
    {
        outInfo.estimated_in_epoch = parseIso8601(arrStr);
        DBG_INFO("AeroAPI: %s arr=\"%s\" epoch=%lu delta=%ldmin",
                 flightIdent.c_str(), arrStr,
                 (unsigned long)outInfo.estimated_in_epoch,
                 (long)(outInfo.estimated_in_epoch - nowSel) / 60);
    }

    return true;
}
