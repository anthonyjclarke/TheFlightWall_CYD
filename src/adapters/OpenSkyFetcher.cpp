/*
Purpose: Fetch ADS-B state vectors from OpenSky Network (OAuth-protected API).
Responsibilities:
- Manage OAuth2 client_credentials token lifecycle with early refresh.
- Build geographic bounding box around a center point and query states/all.
- Parse JSON into StateVector objects and compute distance/bearing.
- Filter by radius and bearing using GeoUtils helpers.
Inputs: centerLat, centerLon, radiusKm, min/max bearing; APIConfiguration creds/URLs.
Outputs: Populates outStateVectors with filtered results (distance_km, bearing_deg set).
*/
#include "OpenSkyFetcher.h"
#include "RuntimeConfig.h"
#include "debug.h"

static String urlEncodeForm(const String &value)
{
    String out;
    const char *hex = "0123456789ABCDEF";
    for (size_t i = 0; i < value.length(); ++i)
    {
        char c = value[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
        {
            out += c;
        }
        else if (c == ' ')
        {
            out += '+';
        }
        else
        {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

bool OpenSkyFetcher::ensureAccessToken(bool forceRefresh)
{
    const bool oauthConfigured = (RuntimeConfig::openskyClientId().length() > 0) &&
                                 (RuntimeConfig::openskyClientSecret().length() > 0);
    if (!oauthConfigured)
    {
        DBG_WARN("OpenSky: OAuth credentials not configured");
        return false;
    }

    unsigned long nowMs = millis();
    const unsigned long safetySkewMs = 60UL * 1000UL; // refresh 60s before expiry
    if (!forceRefresh && m_accessToken.length() > 0 && nowMs + safetySkewMs < m_tokenExpiryMs)
    {
        DBG_VERBOSE("OpenSky: cached token valid, ms to refresh: %ld",
                    (long)(m_tokenExpiryMs - safetySkewMs - nowMs));
        return true;
    }

    DBG_INFO("%s", forceRefresh ? "OpenSky: token refresh (forced)" : "OpenSky: fetching new token");
    String newToken;
    unsigned long newExpiryMs = 0;
    if (!requestAccessToken(newToken, newExpiryMs))
    {
        DBG_ERROR("OpenSky: failed to obtain OAuth token");
        return false;
    }

    m_accessToken   = newToken;
    m_tokenExpiryMs = newExpiryMs;
    DBG_INFO("OpenSky: token cached, expires at ms: %ld", (long)m_tokenExpiryMs);
    return true;
}

bool OpenSkyFetcher::ensureAuthenticated(bool forceRefresh)
{
    return ensureAccessToken(forceRefresh);
}

bool OpenSkyFetcher::requestAccessToken(String &outToken, unsigned long &outExpiryMs)
{
    const String clientId     = RuntimeConfig::openskyClientId();
    const String clientSecret = RuntimeConfig::openskyClientSecret();

    if (clientId.length() == 0 || clientSecret.length() == 0)
    {
        DBG_WARN("OpenSky: OAuth credentials not configured");
        return false;
    }

    HTTPClient http;
    DBG_VERBOSE("OpenSky: token URL: %s", APIConfiguration::OPENSKY_TOKEN_URL);
    http.begin(APIConfiguration::OPENSKY_TOKEN_URL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Accept", "application/json");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    String body = String("grant_type=client_credentials&client_id=") +
                  urlEncodeForm(clientId) +
                  "&client_secret=" +
                  urlEncodeForm(clientSecret);

    DBG_VERBOSE("OpenSky: client_id: %s  secret_len: %d  body_len: %d",
                clientId.c_str(),
                (int)clientSecret.length(),
                (int)body.length());
    http.setTimeout(15000);

    int code = http.POST(body);
    String payload = http.getString();
    if (code != 200)
    {
        DBG_WARN("OpenSky: token POST failed, code: %d  payload: %s",
                 code, payload.length() ? payload.c_str() : "<empty>");
        http.end();
        return false;
    }
    http.end();

    DynamicJsonDocument doc(12288);
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
    {
        DBG_ERROR("OpenSky: token JSON parse error: %s", err.c_str());
        return false;
    }

    String tokenStr = doc["access_token"].as<String>();
    int expiresIn   = doc["expires_in"] | 1800; // default 30 min
    if (tokenStr.length() == 0)
    {
        DBG_ERROR("OpenSky: access_token missing in response");
        DBG_VERBOSE("OpenSky: full response: %s", payload.c_str());
        if (doc.is<JsonObject>())
        {
            for (JsonPair kv : doc.as<JsonObject>())
                DBG_VERBOSE("OpenSky:  key: %s", kv.key().c_str());
        }
        return false;
    }

    outToken    = tokenStr;
    outExpiryMs = millis() + (unsigned long)expiresIn * 1000UL;
    DBG_INFO("OpenSky: token obtained, len: %d  expires_in: %ds", (int)outToken.length(), expiresIn);
    return true;
}

// Parse a single raw states array element into a StateVector, filtering by radius.
// Returns false if the element should be skipped.
static bool parseStateVector(JsonVariant v, double centerLat, double centerLon,
                             double radiusKm, StateVector &out)
{
    if (!v.is<JsonArray>())
    {
        DBG_VERBOSE("OpenSky: expected array element in states");
        return false;
    }
    JsonArray a = v.as<JsonArray>();
    if (a.size() < 17)
    {
        DBG_VERBOSE("OpenSky: state vector has < 17 elements");
        return false;
    }

    out.icao24          = a[0].as<const char *>();
    out.callsign        = a[1].isNull() ? String("") : String(a[1].as<const char *>());
    out.callsign.trim();
    out.origin_country  = a[2].isNull() ? String("") : String(a[2].as<const char *>());
    out.time_position   = a[3].isNull()  ? 0     : a[3].as<long>();
    out.last_contact    = a[4].isNull()  ? 0     : a[4].as<long>();
    out.lon             = a[5].isNull()  ? NAN   : a[5].as<double>();
    out.lat             = a[6].isNull()  ? NAN   : a[6].as<double>();
    out.baro_altitude   = a[7].isNull()  ? NAN   : a[7].as<double>();
    out.on_ground       = a[8].isNull()  ? false : a[8].as<bool>();
    out.velocity        = a[9].isNull()  ? NAN   : a[9].as<double>();
    out.heading         = a[10].isNull() ? NAN   : a[10].as<double>();
    out.vertical_rate   = a[11].isNull() ? NAN   : a[11].as<double>();
    out.sensors         = a[12].isNull() ? 0     : a[12].as<long>();
    out.geo_altitude    = a[13].isNull() ? NAN   : a[13].as<double>();
    out.squawk          = a[14].isNull() ? String("") : String(a[14].as<const char *>());
    out.spi             = a[15].isNull() ? false : a[15].as<bool>();
    out.position_source = a[16].isNull() ? 0     : a[16].as<int>();

    if (isnan(out.lat) || isnan(out.lon))
    {
        DBG_VERBOSE("OpenSky: skipping state vector with invalid coordinates");
        return false;
    }

    out.distance_km = haversineKm(centerLat, centerLon, out.lat, out.lon);
    if (out.distance_km > radiusKm)
        return false;

    out.bearing_deg = computeBearingDeg(centerLat, centerLon, out.lat, out.lon);
    return true;
}

static bool parseStatesPayload(const String &payload, double centerLat, double centerLon,
                               double radiusKm, std::vector<StateVector> &out)
{
    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
    {
        DBG_ERROR("OpenSky: JSON parse error: %s", err.c_str());
        return false;
    }

    JsonArray states = doc["states"].as<JsonArray>();
    if (states.isNull())
        return true; // no states is not an error

    for (JsonVariant v : states)
    {
        StateVector s;
        if (parseStateVector(v, centerLat, centerLon, radiusKm, s))
            out.push_back(s);
    }
    return true;
}

bool OpenSkyFetcher::fetchStateVectors(double centerLat,
                                       double centerLon,
                                       double radiusKm,
                                       std::vector<StateVector> &outStateVectors)
{
    if (!ensureAccessToken(false))
    {
        DBG_WARN("OpenSky: ensureAccessToken failed — aborting fetch");
        return false;
    }

    double latMin, latMax, lonMin, lonMax;
    centeredBoundingBox(centerLat, centerLon, radiusKm, latMin, latMax, lonMin, lonMax);

    String url = String(APIConfiguration::OPENSKY_BASE_URL) +
                 "/api/states/all?lamin=" + String(latMin, 6) +
                 "&lamax=" + String(latMax, 6) +
                 "&lomin=" + String(lonMin, 6) +
                 "&lomax=" + String(lonMax, 6);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + m_accessToken);

    int code = http.GET();

    if (code == 401 && m_accessToken.length() > 0)
    {
        // Token rejected — refresh once and retry
        http.end();
        if (!ensureAccessToken(true))
        {
            DBG_WARN("OpenSky: token refresh failed after 401");
            return false;
        }
        HTTPClient retry;
        retry.begin(url);
        retry.addHeader("Authorization", String("Bearer ") + m_accessToken);
        code = retry.GET();
        if (code != 200)
        {
            DBG_WARN("OpenSky: retry after token refresh failed, code: %d", code);
            retry.end();
            return false;
        }
        String payload = retry.getString();
        retry.end();
        return parseStatesPayload(payload, centerLat, centerLon, radiusKm, outStateVectors);
    }

    if (code != 200)
    {
        DBG_WARN("OpenSky: HTTP GET failed, code: %d", code);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();
    return parseStatesPayload(payload, centerLat, centerLon, radiusKm, outStateVectors);
}
