#include <vector>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include "debug.h"
#include "RuntimeConfig.h"
#include "TimingConfiguration.h"
#include "HardwareConfiguration.h"
#include "Version.h"
#include "OpenSkyFetcher.h"
#include "AeroAPIFetcher.h"
#include "FlightDataFetcher.h"
#include "CYDDisplay.h"
#include "MapProvider.h"
#include "WebUIServer.h"

static OpenSkyFetcher    g_openSky;
static AeroAPIFetcher    g_aeroApi;
static FlightDataFetcher *g_fetcher = nullptr;
static CYDDisplay        g_display;
static WebUIServer       g_webUI;

struct FetchCtx { WebUIServer *web; CYDDisplay *disp; };
static FetchCtx g_fetchCtx;

static unsigned long g_lastFetchMs    = 0;
static unsigned long g_rebootAt       = 0; // non-zero = reboot pending at this millis() value
static unsigned long g_firstFetchAt   = 0; // millis() value after which the first fetch is allowed
static std::vector<StateVector> g_states;
static std::vector<FlightInfo>  g_flights;
static String g_emptyMessage; // non-empty = show this on TFT instead of "Searching..."

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
  // Splash JPEG from LittleFS replaces the legacy "FlightWall" status text.
  // If the splash is missing (LittleFS image not flashed) fall back to the
  // text banner so the device never boots to a blank screen.
  if (!g_display.showSplash())
    g_display.displayMessage("FlightWall");
}

static void initWiFi()
{
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setSaveConnectTimeout(20);
  g_display.showFetchStatus("Connecting WiFi...");
  if (wm.autoConnect(HardwareConfiguration::WIFI_AP_NAME))
  {
    DBG_INFO("WiFi connected: %s", WiFi.localIP().toString().c_str());
    const String ok = "WiFi OK: " + WiFi.localIP().toString();
    g_display.showFetchStatus(ok.c_str());
    delay(1500);
  }
  else
  {
    DBG_WARN("WiFi portal timed out — running without network");
    g_display.showFetchStatus("No WiFi");
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
  DBG_INFO("FlightWall CYD firmware v%s booting", FW_VERSION_STR);
  RuntimeConfig::load();
  initFilesystem();
  initDisplay();
  // Hold the splash on screen for ≥1.5 s before WiFi/NTP status messages
  // overwrite it. Per branding spec; OK to delay in setup().
  delay(1500);
  initWiFi();
  initTime();
  g_webUI.begin(&g_flights, &g_display);
  g_fetcher = new FlightDataFetcher(&g_openSky, &g_aeroApi);
  g_fetchCtx = {&g_webUI, &g_display};
  g_fetcher->setProgressCallback(
    [](void *ctx, const char *phase) {
      auto *c = static_cast<FetchCtx *>(ctx);
      c->web->setBusy(phase && phase[0] != '\0', phase);
      c->web->pump();
      c->disp->showFetchStatus(phase);
    },
    &g_fetchCtx);
  g_firstFetchAt = millis() + TimingConfiguration::STARTUP_WEBUI_GRACE_MS;
  // Pre-arm the interval timer so the first fetch fires at grace-expiry, not after
  // a full interval. Unsigned wrap-around arithmetic is intentional: at the moment
  // now == g_firstFetchAt, (now - g_lastFetchMs) will equal exactly intervalMs.
  g_lastFetchMs  = g_firstFetchAt - RuntimeConfig::fetchIntervalSec() * 1000UL;
  DBG_INFO("WebUI ready — first fetch in %u s (http://%s/)",
           TimingConfiguration::STARTUP_WEBUI_GRACE_MS / 1000,
           WiFi.localIP().toString().c_str());
  // Splash + bottom-bar status holds until the first flight card is drawn.
  // No fullscreen showLoading() here — that would clobber the splash.
  g_display.showFetchStatus("Searching...");
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
  const bool shouldFetch = (now >= g_firstFetchAt) && (now - g_lastFetchMs >= intervalMs);

  if (shouldFetch && WiFi.status() != WL_CONNECTED)
  {
    DBG_WARN("WiFi not connected — skipping fetch");
    // If no flight data is showing, paint the splash; otherwise leave last-good
    // flight cards visible. Either way the message lives in the bottom bar.
    if (g_flights.empty()) g_display.showSplash();
    g_display.showFetchStatus("No WiFi");
    g_lastFetchMs = now;
    return;
  }

  if (shouldFetch)
  {
    std::vector<StateVector> states;
    std::vector<FlightInfo>  flights;
    g_display.showFetchStatus("Fetching...");
    g_webUI.setBusy(true, "Starting fetch");
    const size_t enriched = g_fetcher->fetchFlights(states, flights);
    g_webUI.setBusy(false, "");
    // Don't clear the TFT bar here — the loop tail will repaint with the
    // next steady-state phase ("Searching...", empty msg, or first flight
    // card) and avoid a black flash at the bottom of the screen.

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

    const String apiErr = g_openSky.lastError();
    g_webUI.setApiError(apiErr);
    g_webUI.setCreditsRemaining(g_openSky.creditsRemaining());

    if (!flights.empty())
    {
      g_states  = states;
      g_flights = flights;
      g_emptyMessage = "";
    }
    else if (!states.empty())
    {
      // Aircraft were observed within radius but the displayability filter
      // (FlightDataFetcher) removed all of them — typically parked aircraft or
      // non-airliner transponder targets. Drop any stale last-good list so
      // the TFT reflects current reality, not a cached flight from minutes ago.
      g_flights.clear();
      g_states = states;
      g_emptyMessage = apiErr.length() ? apiErr :
                       String("No active flights within ") + String((int)RuntimeConfig::radiusKm()) + "km";
    }
    else if (g_flights.empty())
    {
      // Fetch returned no state vectors and no last-good data available.
      g_states.clear();
      g_emptyMessage = apiErr.length() ? apiErr :
                       String("No active flights within ") + String((int)RuntimeConfig::radiusKm()) + "km";
    }
    // else: fetch returned nothing but we still have last-good g_flights — keep showing it.

    g_webUI.recordFetch(states, flights, enriched);
    // Pre-fetch / validate the map cache — returns immediately on a 24-hour cache hit.
    g_display.showFetchStatus("Map cache");
    g_webUI.setBusy(true, "Map cache");
    MapProvider::ensureMapCached(g_display.width(), g_display.height());
    g_webUI.setBusy(false, "");
    // Bar holds "Map cache" until loop tail repaints — see note above.
    g_lastFetchMs = millis();
  }

  // ── Normal display routing ─────────────────────────────────────────────────
  // Two states: flight cards (when we have data) OR splash + bottom-bar status
  // (when we don't). The bar carries whatever the most relevant message is —
  // "No active flights within Nkm", "Searching...", "No WiFi", etc.
  // Both showSplash() and showFetchStatus() are idempotent on identical state
  // so calling them every loop tick is essentially free.
  if (!g_flights.empty())
  {
    g_display.displayFlights(g_flights);
  }
  else
  {
    g_display.showSplash();
    g_display.showFetchStatus(g_emptyMessage.length() ? g_emptyMessage.c_str()
                                                      : "Searching...");
  }
}
