#include <vector>
#include <algorithm>
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
#include "FlightWallFetcher.h"
#include "GeoUtils.h"
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
  g_display.setBrightness(RuntimeConfig::brightness()); // honour saved brightness at boot
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

// ── Pinned flight: promote from existing list or fetch separately ─────────────
// Called once per fetch cycle after g_flights has been resolved to its
// last-good-or-new state. Prepends the pinned card at slot 0.
static void promoteOrFetchPinned(std::vector<FlightInfo> &flights)
{
  String pinned = RuntimeConfig::pinnedFlightNumber();
  pinned.trim();
  pinned.toUpperCase();
  if (pinned.length() == 0)
  {
    // Pinned flight cleared — strip any stale pinned card so it disappears
    // immediately rather than lingering via last-good retention.
    flights.erase(std::remove_if(flights.begin(), flights.end(),
                                 [](const FlightInfo &f) { return f.pinned; }),
                  flights.end());
    return;
  }

  // Case-insensitive match against all known identifiers for a flight.
  auto matchesPinned = [&](const FlightInfo &f) -> bool {
    String a = f.ident;      a.toUpperCase();
    String b = f.ident_iata; b.toUpperCase();
    String c = f.ident_icao; c.toUpperCase();
    return a == pinned || b == pinned || c == pinned;
  };

  // ── Step 1: pinned flight already in the normal in-radius list? ───────────
  auto it = std::find_if(flights.begin(), flights.end(), matchesPinned);
  if (it != flights.end())
  {
    it->pinned = true;
    std::rotate(flights.begin(), it, it + 1);
    DBG_INFO("Pinned: %s promoted to slot 0 (in radius)", pinned.c_str());
    return;
  }

  // ── Step 2: not in list — enrich via AeroAPI ──────────────────────────────
  FlightInfo info;
  g_display.showFetchStatus("Pinned flight");
  if (!g_aeroApi.fetchFlightInfo(pinned, info))
  {
    // No active record yet (pre-departure, cancelled, or wrong callsign).
    // Insert a placeholder so the TFT always shows that a pinned flight is
    // configured, rather than silently having no card.
    FlightInfo ph;
    ph.ident  = pinned;
    ph.pinned = true;
    ph.airline_display_name_full = "Awaiting data...";
    if (!flights.empty() && flights[0].pinned)
      flights[0] = ph;
    else
      flights.insert(flights.begin(), ph);
    DBG_INFO("Pinned: %s not active — placeholder at slot 0", pinned.c_str());
    return;
  }
  info.enriched = true;

  // AeroAPI may have resolved to the ICAO ident — check for duplicate again.
  auto it2 = std::find_if(flights.begin(), flights.end(), matchesPinned);
  if (it2 != flights.end())
  {
    it2->pinned = true;
    std::rotate(flights.begin(), it2, it2 + 1);
    DBG_INFO("Pinned: %s matched by ICAO ident after AeroAPI", info.ident.c_str());
    return;
  }

  // ── Step 3: live position via AeroAPI /flights/search ─────────────────────
  // /flights/{ident} (used above for route/timing) carries no position; the
  // search endpoint returns last_position for airborne flights. OpenSky cannot be
  // queried by callsign (/states/all has no callsign filter), so this is the
  // position source for an out-of-radius pinned flight. Returns false (leaving
  // lat/lon NaN → "Locating…") when the flight is not yet airborne.
  //
  // AeroAPI /flights/search indexes by ATC (ICAO) callsign. If the user entered
  // an IATA ident (e.g. "GA714"), info.ident may be returned as IATA while the
  // search only matches ICAO ("GIA714"). Try all three ident forms in sequence.
  bool posOk = g_aeroApi.fetchLivePosition(info.ident, info);
  if (!posOk && info.ident_icao.length() && info.ident_icao != info.ident)
    posOk = g_aeroApi.fetchLivePosition(info.ident_icao, info);
  if (!posOk && info.ident_iata.length() &&
      info.ident_iata != info.ident && info.ident_iata != info.ident_icao)
    posOk = g_aeroApi.fetchLivePosition(info.ident_iata, info);

  if (posOk)
  {
    info.distance_km = haversineKm(RuntimeConfig::centerLat(), RuntimeConfig::centerLon(),
                                   info.lat, info.lon);
    info.bearing_deg = computeBearingDeg(RuntimeConfig::centerLat(), RuntimeConfig::centerLon(),
                                         info.lat, info.lon);
    DBG_INFO("Pinned: %s position from AeroAPI (%.0f km)", info.ident.c_str(), info.distance_km);
  }
  else
  {
    DBG_INFO("Pinned: %s no live position from AeroAPI (tried: %s / %s / %s) — pre-departure, on ground, or coverage gap",
             info.ident.c_str(), info.ident.c_str(), info.ident_icao.c_str(), info.ident_iata.c_str());
  }

  // ── Step 4: FlightWall enrichment (logos are LittleFS-cached) ─────────────
  FlightWallFetcher fw;
  if (info.operator_icao.length())
  {
    String airlineFull; uint16_t airlineColor;
    if (fw.getAirlineData(info.operator_icao, airlineFull, airlineColor))
    {
      info.airline_display_name_full = airlineFull;
      info.airline_color = airlineColor;
    }
    String logoPath;
    if (fw.getAirlineLogo(info.operator_icao, info.operator_iata, logoPath))
      info.logo_path = logoPath;
  }
  if (info.aircraft_code.length())
  {
    String sh, full;
    if (fw.getAircraftName(info.aircraft_code, sh, full) && sh.length())
      info.aircraft_display_name_short = sh;
  }

  info.pinned = true;
  flights.insert(flights.begin(), info);
  DBG_INFO("Pinned: %s added as slot 0 (out of radius)", info.ident.c_str());
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

  // ── Pinned flight changed in WebUI (user clicked "Update") ───────────────────
  // Act immediately so the TFT and dashboard reflect the change without waiting
  // for the next fetch interval.  g_lastFetchMs = 0 forces shouldFetch true this
  // same iteration.
  if (g_webUI.takePendingForceFetch())
  {
    String fc = RuntimeConfig::pinnedFlightNumber();
    fc.trim();

    // Always strip any existing pinned card first — covers both re-pinning a
    // different flight and clearing the pin entirely (fixes stale-card lag).
    g_flights.erase(std::remove_if(g_flights.begin(), g_flights.end(),
                                   [](const FlightInfo &f) { return f.pinned; }),
                    g_flights.end());

    if (fc.length() > 0)
    {
      // Fetch the pinned flight's data immediately (AeroAPI route + live position
      // + FlightWall enrichment) rather than inserting a placeholder and waiting
      // for the full normal fetch cycle (which may run 10+ AeroAPI calls first).
      g_webUI.setBusy(true, "Pinned flight");
      promoteOrFetchPinned(g_flights);
      g_webUI.setBusy(false, "");
      DBG_INFO("Pinned force fetch: immediate enrichment done for %s", fc.c_str());
    }
    else
    {
      DBG_INFO("Pinned cleared: stale card stripped");
    }

    g_display.resetRenderState();
    if (!g_flights.empty())
      g_display.displayFlights(g_flights);
    // No g_lastFetchMs reset here — the pinned data is already fetched
    // immediately above; triggering a full OpenSky+AeroAPI cycle just because
    // the pin changed caused double promoteOrFetchPinned calls and an
    // unnecessary extra fetch cycle. The normal interval handles in-radius refresh.
  }

  // ── Live settings change (user clicked "Save") ───────────────────────────────
  // Settings apply without a reboot: brightness is written to the backlight,
  // OpenSky credentials force a token refresh, and a fresh fetch is triggered so
  // location / radius / timing / label-colour changes show within seconds.
  if (g_webUI.takePendingReauth())
  {
    g_openSky.invalidateToken();
    DBG_INFO("Settings: OpenSky credentials changed — token invalidated");
  }
  if (g_webUI.takePendingApply())
  {
    g_display.setBrightness(RuntimeConfig::brightness());
    g_display.resetRenderState(); // label colour / layout may have changed
    g_lastFetchMs = 0;            // refresh promptly with the new settings
    DBG_INFO("Settings: applied live (brightness, label colour, location, timing)");
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
    // If a pinned-flight POST arrived via pump() while this fetch was already
    // in flight, _pendingForceFetch will be set. The promoteOrFetchPinned() call
    // at the end of this block handles it — consume the flag now so the force-
    // fetch handler doesn't fire a second immediate call on the next tick.
    (void)g_webUI.takePendingForceFetch();

    std::vector<StateVector> states;
    std::vector<FlightInfo>  flights;
    const bool isForceFetch = (g_lastFetchMs == 0);
    g_display.showFetchStatus(isForceFetch ? "Refreshing..." : "Fetching...");
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

    // Promote or fetch the user-configured pinned flight (no-op if not set).
    g_webUI.setBusy(true, "Pinned flight");
    promoteOrFetchPinned(g_flights);
    g_webUI.setBusy(false, "");

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
