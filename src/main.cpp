#include <vector>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include "debug.h"
#include "RuntimeConfig.h"
#include "TimingConfiguration.h"
#include "HardwareConfiguration.h"
#include "OpenSkyFetcher.h"
#include "AeroAPIFetcher.h"
#include "FlightDataFetcher.h"
#include "CYDDisplay.h"
#include "WebUIServer.h"

static OpenSkyFetcher    g_openSky;
static AeroAPIFetcher    g_aeroApi;
static FlightDataFetcher *g_fetcher = nullptr;
static CYDDisplay        g_display;
static WebUIServer       g_webUI;

static unsigned long g_lastFetchMs = 0;
static unsigned long g_rebootAt    = 0; // non-zero = reboot pending at this millis() value
static std::vector<StateVector> g_states;
static std::vector<FlightInfo>  g_flights;

static void initFilesystem()
{
  // LittleFS.begin(true) formats the partition on first boot if it is not yet initialised.
  if (!LittleFS.begin(true))
  {
    DBG_ERROR("LittleFS mount failed");
    return;
  }
  LittleFS.mkdir("/logos"); // no-op if already exists; ensures dir is present
  DBG_INFO("LittleFS mounted — free: %u bytes", LittleFS.totalBytes() - LittleFS.usedBytes());
}

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

static void initTime()
{
  configTzTime(TimingConfiguration::LOCAL_TIMEZONE, "pool.ntp.org", "time.nist.gov");
  DBG_INFO("Waiting for NTP sync...");
  time_t t = 0;
  const unsigned long start = millis();
  while (t < 1000000000UL && millis() - start < 10000)
  {
    delay(100);
    t = time(nullptr);
  }
  if (t > 1000000000UL)
    DBG_INFO("NTP synced: epoch=%lu timezone=Australia/Sydney", (unsigned long)t);
  else
    DBG_WARN("NTP sync timed out — flight status lines unavailable");
}

void setup()
{
  Serial.begin(115200);
  delay(200); // settle before initialising peripherals
  RuntimeConfig::load();
  initFilesystem();
  initDisplay();
  initWiFi();
  initTime();
  g_webUI.begin(&g_flights, &g_display);
  g_fetcher = new FlightDataFetcher(&g_openSky, &g_aeroApi);
  g_display.showLoading();
  DBG_INFO("Free heap: %u bytes", ESP.getFreeHeap());
}

void loop()
{
  // ── WebUI / reboot handling ────────────────────────────────────────────────
  g_webUI.handle();

  if (g_webUI.shouldReboot() && g_rebootAt == 0)
  {
    g_rebootAt = millis() + 400; // allow TCP flush before restart
    g_display.displayMessage("Config saved — rebooting...");
  }
  if (g_rebootAt > 0 && millis() >= g_rebootAt)
  {
    DBG_INFO("Rebooting after config save");
    ESP.restart();
  }

  // ── Flight fetch / display ─────────────────────────────────────────────────
  const unsigned long now        = millis();
  const unsigned long intervalMs = RuntimeConfig::fetchIntervalSec() * 1000UL;
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

    g_webUI.recordFetch(states, flights, enriched);
    g_lastFetchMs = millis();
  }

  g_display.displayFlights(g_flights);
}
