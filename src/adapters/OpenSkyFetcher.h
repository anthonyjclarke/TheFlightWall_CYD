#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "BaseStateVectorFetcher.h"
#include "GeoUtils.h"
#include "APIConfiguration.h"
#include "UserConfiguration.h"

class OpenSkyFetcher : public BaseStateVectorFetcher
{
public:
    OpenSkyFetcher() = default;
    ~OpenSkyFetcher() override = default;

    bool fetchStateVectors(double centerLat,
                           double centerLon,
                           double radiusKm,
                           std::vector<StateVector> &outStateVectors) override;

    // Fetch a single state vector by ATC callsign, anywhere in the world.
    // distance_km and bearing_deg are computed from the configured centre.
    // Returns false if not found, rate-limited, or auth fails.
    bool fetchByCallsign(const String &callsign, StateVector &out);

    bool ensureAuthenticated(bool forceRefresh = false);

    // Drop the cached OAuth token so the next fetch re-authenticates. Call after
    // OpenSky credentials change at runtime so new creds take effect immediately.
    void invalidateToken() { m_accessToken = ""; m_tokenExpiryMs = 0; }

    const String& lastError()      const { return m_lastError; }
    int           creditsRemaining() const { return m_creditsRemaining; }

private:
    String m_accessToken;
    String m_lastError;
    unsigned long m_tokenExpiryMs       = 0;
    unsigned long m_rateLimitedUntilMs  = 0; // non-zero = honour 429 Retry-After window
    int           m_creditsRemaining    = -1; // -1 = not yet observed

    bool ensureAccessToken(bool forceRefresh = false);
    bool requestAccessToken(String &outToken, unsigned long &outExpiryMs);
    void updateCreditsFromHeader(HTTPClient &http);
};
