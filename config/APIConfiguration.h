#pragma once

#include <Arduino.h>

// Credentials are loaded from include/secrets.h (gitignored).
// Copy include/secrets.h.template to include/secrets.h and fill in values.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace APIConfiguration
{
    // OpenSky Network — OAuth2 client credentials
#ifdef SECRET_OPENSKY_CLIENT_ID
    static const char *OPENSKY_CLIENT_ID     = SECRET_OPENSKY_CLIENT_ID;
    static const char *OPENSKY_CLIENT_SECRET = SECRET_OPENSKY_CLIENT_SECRET;
#else
    static const char *OPENSKY_CLIENT_ID     = "";
    static const char *OPENSKY_CLIENT_SECRET = "";
#endif

    static constexpr const char *OPENSKY_TOKEN_URL =
        "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";
    static constexpr const char *OPENSKY_BASE_URL = "https://opensky-network.org";

    // FlightAware AeroAPI
#ifdef SECRET_AEROAPI_KEY
    static const char *AEROAPI_KEY = SECRET_AEROAPI_KEY;
#else
    static const char *AEROAPI_KEY = "";
#endif

    static constexpr const char *AEROAPI_BASE_URL = "https://aeroapi.flightaware.com/aeroapi";

    // FlightWall CDN — airline/aircraft name lookup
    static constexpr const char *FLIGHTWALL_CDN_BASE_URL = "https://cdn.theflightwall.com";

    // TLS — set false in production once CA cert is pinned
    static constexpr bool AEROAPI_INSECURE_TLS    = true;
    static constexpr bool FLIGHTWALL_INSECURE_TLS = true;
}
