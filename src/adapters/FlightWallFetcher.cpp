/*
Purpose: Look up human-friendly airline and aircraft names from FlightWall CDN.
Responsibilities:
- HTTPS GET small JSON blobs for airline/aircraft codes and parse display names.
- Download and LittleFS-cache airline logo JPEGs for display.
- Provide helpers used by FlightDataFetcher for user-facing labels.
Inputs: Airline ICAO code or aircraft ICAO type.
Outputs: Display name strings (short/full) via out parameters.
*/
#include "FlightWallFetcher.h"
#include "debug.h"

bool FlightWallFetcher::httpGetJson(const String &url, String &outPayload)
{
    WiFiClientSecure client;
    if (APIConfiguration::FLIGHTWALL_INSECURE_TLS)
    {
        client.setInsecure();
    }

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if (code != 200)
    {
        http.end();
        return false;
    }
    outPayload = http.getString();
    http.end();
    return true;
}

bool FlightWallFetcher::getAirlineData(const String &airlineIcao,
                                       String &outDisplayNameFull,
                                       uint16_t &outColorRgb565)
{
    outDisplayNameFull = String("");
    outColorRgb565 = 0xFFFF; // white fallback
    if (airlineIcao.length() == 0)
        return false;

    String url = String(APIConfiguration::FLIGHTWALL_CDN_BASE_URL) + "/oss/lookup/airline/" + airlineIcao + ".json";
    String payload;
    if (!httpGetJson(url, payload))
        return false;

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
        return false;

    if (doc.containsKey("display_name_full"))
        outDisplayNameFull = String(doc["display_name_full"].as<const char *>());

    if (!doc["brand_color_hex"].isNull())
    {
        const char *hex = doc["brand_color_hex"].as<const char *>();
        if (hex && hex[0] == '#' && strlen(hex) >= 7)
        {
            unsigned int r = 0, g = 0, b = 0;
            sscanf(hex + 1, "%2x%2x%2x", &r, &g, &b);
            outColorRgb565 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        }
    }

    return outDisplayNameFull.length() > 0;
}

bool FlightWallFetcher::httpGetToFile(const String &url, const String &lfsPath)
{
    WiFiClientSecure client;
    if (APIConfiguration::FLIGHTWALL_INSECURE_TLS)
        client.setInsecure();

    HTTPClient http;
    http.begin(client, url);

    int code = http.GET();
    if (code != 200)
    {
        DBG_WARN("Logo GET %d for %s", code, url.c_str());
        http.end();
        return false;
    }

    File f = LittleFS.open(lfsPath, FILE_WRITE, true);
    if (!f)
    {
        DBG_ERROR("LittleFS open failed: %s", lfsPath.c_str());
        http.end();
        return false;
    }

    int written = http.writeToStream(&f);
    f.close();
    http.end();

    if (written <= 0)
    {
        LittleFS.remove(lfsPath);
        return false;
    }

    DBG_INFO("Logo cached: %s (%d bytes)", lfsPath.c_str(), written);
    return true;
}

bool FlightWallFetcher::getAirlineLogo(const String &airlineIcao,
                                       const String &airlineIata,
                                       String &outLfsPath)
{
    outLfsPath = "";

    // Try each code in order: ICAO first, then IATA if the ICAO download fails.
    // Airline logos are named by ICAO in flightaware_logos/ but some airlines
    // (e.g. Qantas QFA) are indexed by their 2-letter IATA code (QF) in the repo.
    const String codes[] = { airlineIcao, airlineIata };
    for (int i = 0; i < 2; i++)
    {
        const String &code = codes[i];
        if (code.length() == 0 || (i == 1 && code == airlineIcao))
            continue;

        const String lfsPath = String("/logos/") + code + ".jpg";
        if (LittleFS.exists(lfsPath))
        {
            outLfsPath = lfsPath;
            return true;
        }

        const String url = String(APIConfiguration::AIRLINE_LOGO_PROXY_BASE) +
                           code + ".png" +
                           "&w=" + String(APIConfiguration::AIRLINE_LOGO_W) +
                           "&h=" + String(APIConfiguration::AIRLINE_LOGO_H);
        if (httpGetToFile(url, lfsPath))
        {
            outLfsPath = lfsPath;
            return true;
        }
    }
    return false;
}

bool FlightWallFetcher::getAircraftName(const String &aircraftIcao,
                                        String &outDisplayNameShort,
                                        String &outDisplayNameFull)
{
    outDisplayNameShort = String("");
    outDisplayNameFull = String("");
    if (aircraftIcao.length() == 0)
        return false;

    String url = String(APIConfiguration::FLIGHTWALL_CDN_BASE_URL) + "/oss/lookup/aircraft/" + aircraftIcao + ".json";
    String payload;
    if (!httpGetJson(url, payload))
        return false;

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
        return false;

    if (doc.containsKey("display_name_short"))
    {
        outDisplayNameShort = String(doc["display_name_short"].as<const char *>());
    }
    if (doc.containsKey("display_name_full"))
    {
        outDisplayNameFull = String(doc["display_name_full"].as<const char *>());
    }
    return outDisplayNameShort.length() > 0 || outDisplayNameFull.length() > 0;
}
