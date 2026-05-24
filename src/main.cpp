#include <vector>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include "debug.h"
#include "config/UserConfiguration.h"
#include "config/TimingConfiguration.h"
#include "config/HardwareConfiguration.h"
#include "adapters/OpenSkyFetcher.h"
#include "adapters/AeroAPIFetcher.h"
#include "core/FlightDataFetcher.h"
#include "adapters/CYDDisplay.h"

static OpenSkyFetcher    g_openSky;
static AeroAPIFetcher    g_aeroApi;
static FlightDataFetcher *g_fetcher = nullptr;
static CYDDisplay        g_display;

static unsigned long g_lastFetchMs = 0;
static std::vector<StateVector> g_states;
static std::vector<FlightInfo>  g_flights;

static void initDisplay()
{
  g_display.initialize();
  g_display.displayMessage("FlightWall");
}

static void initWiFi()
{
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setSaveConnectTimeout(20);
  g_display.displayMessage("Connecting WiFi...");
  if (wm.autoConnect(HardwareConfiguration::WIFI_AP_NAME))
  {
    DBG_INFO("WiFi connected: %s", WiFi.localIP().toString().c_str());
    g_display.displayMessage("WiFi OK: " + WiFi.localIP().toString());
    delay(1500);
  }
  else
  {
    DBG_WARN("WiFi portal timed out — running without network");
    g_display.displayMessage("No WiFi");
    delay(1500);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(200); // settle time before initialising peripherals
  initDisplay();
  initWiFi();
  g_fetcher = new FlightDataFetcher(&g_openSky, &g_aeroApi);
  g_display.showLoading();
  DBG_INFO("Free heap: %u bytes", ESP.getFreeHeap());
}

void loop()
{
  const unsigned long now        = millis();
  const unsigned long intervalMs = TimingConfiguration::FETCH_INTERVAL_SECONDS * 1000UL;
  const bool shouldFetch = g_flights.empty() || (now - g_lastFetchMs >= intervalMs);

  if (shouldFetch && WiFi.status() != WL_CONNECTED)
  {
    DBG_WARN("WiFi not connected — skipping fetch");
    g_display.displayMessage("No WiFi");
    g_lastFetchMs = now;
    return;
  }

  if (shouldFetch)
  {
    std::vector<StateVector> states;
    std::vector<FlightInfo>  flights;
    const size_t enriched = g_fetcher->fetchFlights(states, flights);

    DBG_INFO("State vectors: %u  enriched: %u", (unsigned)states.size(), (unsigned)enriched);

    for (const auto &s : states)
      DBG_VERBOSE("%s @ %.1fkm  %.0fdeg", s.callsign.c_str(), s.distance_km, s.bearing_deg);

    for (const auto &f : flights)
      DBG_INFO("Flight: %s  %s>%s  %s",
               f.ident.c_str(),
               f.origin.code_icao.c_str(),
               f.destination.code_icao.c_str(),
               f.aircraft_display_name_short.length()
                   ? f.aircraft_display_name_short.c_str()
                   : f.aircraft_code.c_str());

    if (!flights.empty())
    {
      g_states  = states;
      g_flights = flights;
    }
    else if (g_flights.empty())
    {
      g_states.clear();
    }

    g_lastFetchMs = millis();
  }

  g_display.displayFlights(g_flights);
}
