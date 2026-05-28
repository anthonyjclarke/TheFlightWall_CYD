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

    bool ensureAuthenticated(bool forceRefresh = false);

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
