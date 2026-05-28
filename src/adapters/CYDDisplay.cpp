#include "CYDDisplay.h"

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <LittleFS.h>
#include <cmath>
#include "UserConfiguration.h"
#include "HardwareConfiguration.h"
#include "RuntimeConfig.h"
#include "MapProvider.h"
#include "debug.h"

// TJpgDec uses a C-style callback with no user data pointer, so we keep a
// file-scope reference to the active TFT instance.
static TFT_eSPI *s_tft_jpeg = nullptr;

static bool jpegOutputCb(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bmp)
{
    if (!s_tft_jpeg || y >= s_tft_jpeg->height()) return false;
    s_tft_jpeg->pushImage(x, y, w, h, bmp);
    return true;
}

// Card layout (320×240 landscape):
//
//  ┌──────────────────────────────────────┐
//  │  1/3           JBU1524               │  ← callsign strip
//  ├──────────────────────────────────────┤
//  │                                      │
//  │  jetBlue       LAX–JFK               │  ← airline logo (colored) | route
//  │                A321neo               │    aircraft below route
//  │                                      │
//  ├──────────────────────────────────────┤
//  │  Departed 45 min ago                 │  ← status lines (or live ADS-B fallback)
//  │  Arriving in 4.5 hr                  │
//  │  ████████████░░░░░░░░░░░░░░░░░░░░░   │  ← progress bar
//  └──────────────────────────────────────┘
//
// Font and layout constants differ between the two supported display variants.
// TFT_HEIGHT is the portrait-mode height (longer dimension):
//   ILI9341 → TFT_HEIGHT=320 (landscape: 320×240)
//   ST7796  → TFT_HEIGHT=480 (landscape: 480×320)
#if TFT_HEIGHT >= 400
  #define F_CALLSIGN  &FreeSansBold24pt7b
  #define F_ROUTE     &FreeSansBold24pt7b
  #define F_AIRLINE   &FreeSansBold18pt7b
  #define F_SUB       &FreeSans12pt7b
  static constexpr int16_t CARD_CALLSIGN_H   = 56;
  static constexpr int16_t CARD_MID_H        = 116;
  static constexpr int16_t CARD_AIRLINE_COL_W = 176;
  static constexpr int16_t CARD_STATUS_H     = 90;
  static constexpr int16_t CARD_BAR_H        = 18;
  static constexpr int16_t CARD_BAR_PAD      = 10;
#else
  #define F_CALLSIGN  &FreeSansBold18pt7b
  #define F_ROUTE     &FreeSansBold18pt7b
  #define F_AIRLINE   &FreeSansBold12pt7b
  #define F_SUB       &FreeSans9pt7b
  static constexpr int16_t CARD_CALLSIGN_H   = 40;
  static constexpr int16_t CARD_MID_H        = 84;
  static constexpr int16_t CARD_AIRLINE_COL_W = 118;
  static constexpr int16_t CARD_STATUS_H     = 64;
  static constexpr int16_t CARD_BAR_H        = 12;
  static constexpr int16_t CARD_BAR_PAD      = 6;
#endif

CYDDisplay::CYDDisplay() {}

CYDDisplay::~CYDDisplay()
{
  if (_tft)
  {
    delete _tft;
    _tft = nullptr;
  }
}

bool CYDDisplay::initialize()
{
  _tft = new TFT_eSPI();
  _tft->init();
  _tft->setRotation(1); // landscape

  _w = _tft->width();
  _h = _tft->height();

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(TFT_BL,
                    HardwareConfiguration::BL_PWM_FREQ,
                    HardwareConfiguration::BL_PWM_BITS,
                    HardwareConfiguration::BL_PWM_CHANNEL);
  ledcWrite(TFT_BL, HardwareConfiguration::BL_BRIGHTNESS);
#else
  ledcSetup(HardwareConfiguration::BL_PWM_CHANNEL,
            HardwareConfiguration::BL_PWM_FREQ,
            HardwareConfiguration::BL_PWM_BITS);
  ledcAttachPin(TFT_BL, HardwareConfiguration::BL_PWM_CHANNEL);
  ledcWrite(HardwareConfiguration::BL_PWM_CHANNEL, HardwareConfiguration::BL_BRIGHTNESS);
#endif

  _tft->fillScreen(UserConfiguration::COLOR_BACKGROUND);

  s_tft_jpeg = _tft;
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true); // ESP32 is little-endian; TFT needs big-endian RGB565
  TJpgDec.setCallback(jpegOutputCb);

  _currentFlightIndex = 0;
  _lastCycleMs        = millis();
  resetRenderState();

  DBG_INFO("CYDDisplay init: %ux%u px", _w, _h);
  return true;
}

void CYDDisplay::clear()
{
  if (_tft)
    _tft->fillScreen(UserConfiguration::COLOR_BACKGROUND);
  resetRenderState();
  _lastStatusMessage = "";
}

void CYDDisplay::displayFlights(const std::vector<FlightInfo> &flights)
{
  if (!_tft)
    return;

  if (flights.empty())
  {
    showLoading();
    return;
  }

  const unsigned long now        = millis();
  const size_t        totalSlots = flights.size() + 1; // +1 for map card

  // Use the map dwell time while the map card is active, card cycle time otherwise
  const bool   currentlyOnMap = (_currentFlightIndex % totalSlots == flights.size());
  const unsigned long intervalMs = (currentlyOnMap
      ? RuntimeConfig::mapDisplaySec()
      : RuntimeConfig::displayCycleSec()) * 1000UL;

  if (totalSlots > 1 && now - _lastCycleMs >= intervalMs)
  {
    _lastCycleMs        = now;
    _currentFlightIndex = (_currentFlightIndex + 1) % totalSlots;
  }

  const size_t idx   = _currentFlightIndex % totalSlots;
  const bool   isMap = (idx == flights.size());
  const String key   = isMap ? mapRenderKey(flights) : renderKey(flights[idx]);

  if (_lastRenderedIndex == idx &&
      _lastRenderedTotal == totalSlots &&
      _lastRenderedKey   == key)
  {
    return;
  }

  if (isMap)
    drawMapCard(flights);
  else
    drawFlightCard(flights[idx], idx + 1, flights.size());

  _lastRenderedIndex = idx;
  _lastRenderedTotal = totalSlots;
  _lastRenderedKey   = key;
  _lastStatusMessage = ""; // leaving the message/loading state — allow next message to redraw
}

void CYDDisplay::displayMessage(const String &message)
{
  if (!_tft) return;
  if (_lastStatusMessage == message) return; // already showing this exact text — skip redraw

  _tft->startWrite();
  _tft->fillScreen(UserConfiguration::COLOR_BACKGROUND);
  _tft->setFreeFont(F_SUB);
  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_MESSAGE, UserConfiguration::COLOR_BACKGROUND);
  _tft->drawString(message, _w / 2, _h / 2);
  _tft->endWrite();
  resetRenderState();
  _lastStatusMessage = message;
}

void CYDDisplay::showLoading()
{
  if (!_tft) return;
  static const char *kLoadingText = "Searching...";
  if (_lastStatusMessage == kLoadingText) return;

  _tft->startWrite();
  _tft->fillScreen(UserConfiguration::COLOR_BACKGROUND);
  _tft->setFreeFont(F_SUB);
  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_SUB, UserConfiguration::COLOR_BACKGROUND);
  _tft->drawString(kLoadingText, _w / 2, _h / 2);
  _tft->endWrite();
  resetRenderState();
  _lastStatusMessage = kLoadingText;
}

String CYDDisplay::renderKey(const FlightInfo &f) const
{
  // Include a per-minute timestamp so status/progress redraws once per minute.
  time_t t = time(nullptr);
  String minuteBucket = (t > 1000000000L) ? String(t / 60) : "";

  return resolveCallsign(f) + "|" +
         resolveIataOrIcao(f.origin) + "|" +
         resolveIataOrIcao(f.destination) + "|" +
         resolveAirline(f) + "|" +
         resolveAircraft(f) + "|" +
         String(f.actual_out_epoch) + "|" +
         String(f.estimated_in_epoch) + "|" +
         resolveLiveSummary(f) + "|" +
         minuteBucket;
}

String CYDDisplay::mapRenderKey(const std::vector<FlightInfo> &flights) const
{
  // Invalidate when: map was re-downloaded (mapVersion bumped), any flight
  // moves by ~1 km (lat/lon rounded to 0.01°), or the minute bucket ticks.
  String key = "MAP:" + String(MapProvider::mapVersion());
  for (const auto &f : flights)
  {
    key += '|';
    if (!isnan(f.lat) && !isnan(f.lon))
    {
      key += String((int)(f.lat * 100));
      key += ',';
      key += String((int)(f.lon * 100));
    }
  }
  time_t t = time(nullptr);
  if (t > 1000000000L) key += '|' + String(t / 60);
  return key;
}

void CYDDisplay::drawMapCard(const std::vector<FlightInfo> &flights)
{
  using namespace UserConfiguration;

  // Ensure a cached map is available (fetches from Google if stale/missing).
  // ensureMapCached() is also called each fetch cycle from main.cpp, so this
  // is normally a fast cache-hit check; the blocking HTTP call happens there.
  const bool hasMap = MapProvider::ensureMapCached(_w, _h);

  // Phase 1 — clear screen, then render JPEG from a heap buffer.
  // drawFsJpg with WiFiClientSecure-written files silently produces no pixels on
  // ESP32-Arduino 3.x (same root cause as getStream() zero-byte issue). Reading
  // the cached file into a uint8_t buffer and using drawJpg() is reliable.
  _tft->startWrite();
  _tft->fillScreen(COLOR_BACKGROUND);
  _tft->endWrite();

  if (hasMap)
  {
    File mapFile = LittleFS.open(MapProvider::cachedMapPath(), "r");
    if (mapFile)
    {
      const size_t fileSize = mapFile.size();
      uint8_t *jpegBuf = (uint8_t *)malloc(fileSize);
      if (jpegBuf)
      {
        mapFile.read(jpegBuf, fileSize);
        mapFile.close();
        JRESULT res = TJpgDec.drawJpg(0, 0, jpegBuf, fileSize);
        if (res != JDR_OK)
          DBG_WARN("CYDDisplay: map JPEG decode error %d  sz=%u heap=%u",
                   (int)res, fileSize, ESP.getFreeHeap());
        free(jpegBuf);
      }
      else
      {
        mapFile.close();
        DBG_WARN("CYDDisplay: no heap for %u-byte map JPEG (free=%u)", fileSize, ESP.getFreeHeap());
      }
    }
  }

  // Phase 2 — header bar + flight marker overlay
  _tft->startWrite();

  // Header: solid black strip for contrast against any map background
  _tft->fillRect(0, 0, _w, 20, 0x0000);
  _tft->setFreeFont(F_SUB);
  _tft->setTextColor(COLOR_SUB, 0x0000);
  _tft->setTextDatum(MR_DATUM);
  String cnt = String(flights.size()) + (flights.size() == 1 ? " flight" : " flights");
  _tft->drawString(cnt, _w - 6, 10);

  if (!hasMap)
  {
    _tft->setTextDatum(MC_DATUM);
    _tft->setTextColor(COLOR_SUB, COLOR_BACKGROUND);
    _tft->drawString("Map unavailable", _w / 2, _h / 2);
    _tft->endWrite();
    return; // render guard in displayFlights() still updates; retry next minute
  }

  // Flight markers — 4-position label collision avoidance.
  // Each placed label's bounding box is recorded; each new label tries
  // above-right → above-left → below-right → below-left and takes the
  // first position that doesn't overlap any already-placed box.
  // If all four collide the label is suppressed; dot and heading tick remain.
  struct LabelBox { int16_t x, y, w, h; };
  LabelBox placed[16];
  int      nPlaced = 0;

  _tft->setFreeFont(F_SUB);
  const int16_t lblH = (int16_t)_tft->fontHeight() + 2;

  for (const auto &f : flights)
  {
    if (isnan(f.lat) || isnan(f.lon)) continue;

    int16_t fx, fy;
    if (!MapProvider::latLonToPixel(f.lat, f.lon, _w, _h, fx, fy)) continue;

    if (fx < 3 || fx >= _w - 3 || fy < 21 || fy >= _h - 3) continue;

    const uint16_t col = f.enriched ? RuntimeConfig::labelColor() : COLOR_MAP_UNENR;

    // Heading tick (direction of travel)
    if (!isnan(f.heading_deg))
    {
      const float   hr = f.heading_deg * (float)PI / 180.0f;
      const int16_t hx = fx + (int16_t)(sinf(hr) * 10.0f);
      const int16_t hy = fy - (int16_t)(cosf(hr) * 10.0f);
      _tft->drawLine(fx, fy, hx, hy, col);
    }

    // Dark outline for contrast over map, then coloured dot
    _tft->fillCircle(fx, fy, 4, 0x0000);
    _tft->fillCircle(fx, fy, 3, col);

    // Callsign label with collision avoidance
    String label = resolveCallsign(f);
    if (label.length() > 6) label = label.substring(0, 6);
    const int16_t lblW = (int16_t)_tft->textWidth(label) + 2;

    // Candidate positions: above-right, above-left, below-right, below-left
    const int16_t candX[4] = { (int16_t)(fx + 7),        (int16_t)(fx - lblW - 5),
                                (int16_t)(fx + 7),        (int16_t)(fx - lblW - 5) };
    const int16_t candY[4] = { (int16_t)(fy - lblH - 2), (int16_t)(fy - lblH - 2),
                                (int16_t)(fy + 5),        (int16_t)(fy + 5) };

    int16_t lx = -1, ly = -1;
    for (int c = 0; c < 4; c++)
    {
      const int16_t cx = candX[c], cy = candY[c];
      if (cx < 0 || cx + lblW > _w || cy < 21 || cy + lblH > _h) continue;
      bool hit = false;
      for (int i = 0; i < nPlaced && !hit; i++)
      {
        const LabelBox &b = placed[i];
        if (cx < b.x + b.w && cx + lblW > b.x && cy < b.y + b.h && cy + lblH > b.y)
          hit = true;
      }
      if (!hit) { lx = cx; ly = cy; break; }
    }

    if (lx >= 0)
    {
      _tft->setTextColor(col);
      _tft->setTextDatum(TL_DATUM);
      _tft->drawString(label, lx, ly);
      if (nPlaced < 16)
        placed[nPlaced++] = { lx, ly, lblW, lblH };
    }
  }

  _tft->endWrite();
}

void CYDDisplay::showStandaloneMap()
{
  static const std::vector<FlightInfo> empty;
  drawMapCard(empty);
  // Reset so when the preview hold expires, normal displayFlights() force-redraws.
  resetRenderState();
}

void CYDDisplay::resetRenderState()
{
  _lastRenderedIndex = (size_t)-1;
  _lastRenderedTotal = 0;
  _lastRenderedKey   = "";
}

// ── Resolve helpers ──────────────────────────────────────────────────────────

String CYDDisplay::resolveAirline(const FlightInfo &f) const
{
  if (f.airline_display_name_full.length()) return f.airline_display_name_full;
  if (f.operator_iata.length())             return f.operator_iata;
  if (f.operator_icao.length())             return f.operator_icao;
  return f.operator_code;
}

String CYDDisplay::resolveCallsign(const FlightInfo &f) const
{
  if (f.ident_iata.length()) return f.ident_iata;
  if (f.ident.length())      return f.ident;
  return f.ident_icao;
}

String CYDDisplay::resolveAircraft(const FlightInfo &f) const
{
  if (f.aircraft_display_name_short.length()) return f.aircraft_display_name_short;
  return f.aircraft_code;
}

String CYDDisplay::resolveIataOrIcao(const AirportInfo &ap) const
{
  if (ap.code_iata.length()) return ap.code_iata;
  if (ap.code_icao.length()) return ap.code_icao;
  return "---";
}

String CYDDisplay::resolveCityOrCode(const AirportInfo &ap) const
{
  if (ap.city.length())      return ap.city;
  if (ap.code_iata.length()) return ap.code_iata;
  if (ap.code_icao.length()) return ap.code_icao;
  return "";
}

static const char *bearingCardinal(double deg)
{
  static const char *dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  if (isnan(deg))
    return "";
  int idx = (int)((deg + 22.5) / 45.0) % 8;
  return dirs[idx];
}

static String formatAltitude(double baroMeters, double geoMeters, bool onGround)
{
  if (onGround)
    return "GROUND";

  double meters = isnan(baroMeters) ? geoMeters : baroMeters;
  if (isnan(meters))
    return "";

  long feet = lround(meters * 3.28084);
  if (feet >= 18000)
    return "FL" + String((int)lround(feet / 100.0));
  return String((int)lround(feet / 100.0) * 100) + "ft";
}

String CYDDisplay::resolveLiveSummary(const FlightInfo &f) const
{
  String s;

  if (!isnan(f.distance_km))
  {
    s += String((int)lround(f.distance_km));
    s += "km";
    const char *dir = bearingCardinal(f.bearing_deg);
    if (dir[0]) { s += " "; s += dir; }
  }

  String alt = formatAltitude(f.baro_altitude_m, f.geo_altitude_m, f.on_ground);
  if (alt.length())
  {
    if (s.length()) s += "  ";
    s += alt;
  }

  return s;
}

String CYDDisplay::resolveMotionSummary(const FlightInfo &f) const
{
  String s;

  if (!isnan(f.velocity_mps))
  {
    s += String((int)lround(f.velocity_mps * 3.6));
    s += "km/h";
  }

  if (!isnan(f.vertical_rate_mps))
  {
    if (fabs(f.vertical_rate_mps) >= 0.5)
    {
      if (s.length()) s += " ";
      s += f.vertical_rate_mps > 0 ? "UP" : "DN";
    }
  }

  if (!isnan(f.heading_deg))
  {
    if (s.length()) s += " ";
    s += String((int)lround(f.heading_deg));
    s += "\xb0"; // degree symbol in Latin-1 (GFX font compatible)
  }

  return s;
}

// ── Status line builders ─────────────────────────────────────────────────────

String CYDDisplay::buildDepartedLine(const FlightInfo &f, time_t now) const
{
  if (f.actual_out_epoch == 0 || now <= f.actual_out_epoch)
    return "";

  const String city   = resolveCityOrCode(f.origin);
  const String prefix = city.length() ? ("Departed " + city + " ") : "Departed ";

  long sec = (long)(now - f.actual_out_epoch);
  if (sec < 3600)
    return prefix + String(sec / 60) + " min ago";

  int hrs    = (int)(sec / 3600);
  int minRem = (int)((sec % 3600) / 60);
  if (minRem == 0)
    return prefix + String(hrs) + " hr ago";
  return prefix + String(hrs) + "h " + String(minRem) + "m ago";
}

String CYDDisplay::buildArrivingLine(const FlightInfo &f, time_t now) const
{
  if (f.estimated_in_epoch == 0)
    return "";

  const String city   = resolveCityOrCode(f.destination);
  const String atCity = city.length() ? (" at " + city) : "";

  if (now >= f.estimated_in_epoch)
    return "Arrived" + atCity;

  long sec = (long)(f.estimated_in_epoch - now);
  if (sec < 3600)
    return "Arriving" + atCity + " in " + String(sec / 60) + " min";

  int hrs    = (int)(sec / 3600);
  int minRem = (int)((sec % 3600) / 60);
  if (minRem == 0)
    return "Arriving" + atCity + " in " + String(hrs) + " hr";
  return "Arriving" + atCity + " in " + String(hrs) + "h " + String(minRem) + "m";
}

// Truncate with ".." until text fits within maxWidth pixels at the current font.
String CYDDisplay::fitText(const String &text, int maxWidth)
{
  if (_tft->textWidth(text) <= maxWidth)
    return text;

  String t = text;
  while (t.length() > 1 && _tft->textWidth(t + "..") > maxWidth)
    t.remove(t.length() - 1);

  return t + "..";
}

// ── Progress bar ─────────────────────────────────────────────────────────────

void CYDDisplay::drawProgressBar(const FlightInfo &f, time_t now)
{
  if (f.estimated_in_epoch <= f.actual_out_epoch) return;

  float progress = 0.0f;
  if (now > f.actual_out_epoch)
  {
    progress = (float)(now - f.actual_out_epoch) /
               (float)(f.estimated_in_epoch - f.actual_out_epoch);
    if (progress > 1.0f) progress = 1.0f;
  }

  const int16_t barX = CARD_BAR_PAD;
  const int16_t barY = _h - CARD_BAR_H - 4;
  const int16_t barW = _w - CARD_BAR_PAD * 2;
  const int16_t fillW = (int16_t)(barW * progress);

  if (fillW > 0)
    _tft->fillRect(barX, barY, fillW, CARD_BAR_H, UserConfiguration::COLOR_PROGRESS);
  if (fillW < barW)
    _tft->fillRect(barX + fillW, barY, barW - fillW, CARD_BAR_H,
                   UserConfiguration::COLOR_PROGRESS_BG);
}

// ── Main card draw ────────────────────────────────────────────────────────────

void CYDDisplay::drawFlightCard(const FlightInfo &f, size_t idx, size_t total)
{
  const int hPad = 6;

  // ── Layout anchors ──────────────────────────────────────────────────────────
  const int16_t divY1     = CARD_CALLSIGN_H;
  const int16_t midTop    = divY1 + 1;
  const int16_t divY2     = midTop + CARD_MID_H;
  const int16_t statusTop = divY2 + 1;

  const int16_t csMidY  = CARD_CALLSIGN_H / 2;
  const int16_t midMidY = midTop + CARD_MID_H / 2;

  // Right-column center X (route/aircraft)
  const int16_t routeColX = CARD_AIRLINE_COL_W + (_w - CARD_AIRLINE_COL_W) / 2;

  // ── Probe logo dimensions before any drawing ────────────────────────────────
  // Done outside startWrite() so LittleFS reads don't interleave with SPI.
  uint16_t logoW = 0, logoH = 0;
  bool hasLogo = false;
  if (f.logo_path.length() && LittleFS.exists(f.logo_path))
  {
    if (TJpgDec.getFsJpgSize(&logoW, &logoH, f.logo_path.c_str(), LittleFS) == JDR_OK
        && logoW > 0 && logoH > 0)
    {
      hasLogo = true;
    }
  }

  const int16_t logoX = CARD_AIRLINE_COL_W / 2 - (int16_t)logoW / 2;
  const int16_t logoY = midMidY - (int16_t)logoH / 2;

  // ── Phase 1: all SPI drawing except the JPEG logo ──────────────────────────
  _tft->startWrite();
  _tft->fillScreen(UserConfiguration::COLOR_BACKGROUND);

  // Counter — small, top-left
  _tft->setFreeFont(F_SUB);
  _tft->setTextDatum(ML_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_SUB, UserConfiguration::COLOR_BACKGROUND);
  _tft->drawString(String(idx) + "/" + String(total), hPad, csMidY);

  // Flight number — large, centered
  _tft->setFreeFont(F_CALLSIGN);
  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_CALLSIGN, UserConfiguration::COLOR_BACKGROUND);
  _tft->drawString(fitText(resolveCallsign(f), _w - 60), _w / 2, csMidY);

  // Dividers
  _tft->drawFastHLine(0, divY1, _w, UserConfiguration::COLOR_DIVIDER);
  _tft->drawFastHLine(0, divY2, _w, UserConfiguration::COLOR_DIVIDER);

  // Airline: colored text if no logo (logo drawn in Phase 2 below)
  if (!hasLogo)
  {
    uint16_t airlineColor = (f.airline_color != 0)
                            ? f.airline_color
                            : UserConfiguration::COLOR_AIRLINE;
    _tft->setFreeFont(F_AIRLINE);
    _tft->setTextDatum(MC_DATUM);
    _tft->setTextColor(airlineColor, UserConfiguration::COLOR_BACKGROUND);
    _tft->drawString(fitText(resolveAirline(f), CARD_AIRLINE_COL_W - hPad * 2),
                     CARD_AIRLINE_COL_W / 2, midMidY);
  }

  // Route — right column, large, amber
  _tft->setFreeFont(F_ROUTE);
  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_ROUTE, UserConfiguration::COLOR_BACKGROUND);
  String route = fitText(resolveIataOrIcao(f.origin) + "-" + resolveIataOrIcao(f.destination),
                         _w - CARD_AIRLINE_COL_W - hPad);
  _tft->drawString(route, routeColX, midTop + CARD_MID_H / 3);

  // Aircraft — right column, below route
  String aircraft = resolveAircraft(f);
  if (aircraft.length())
  {
    _tft->setFreeFont(F_SUB);
    _tft->setTextDatum(MC_DATUM);
    _tft->setTextColor(UserConfiguration::COLOR_SUB, UserConfiguration::COLOR_BACKGROUND);
    _tft->drawString(aircraft, routeColX, midTop + 2 * CARD_MID_H / 3);
  }

  // Status lines
  const int16_t stat1Y = statusTop + CARD_STATUS_H / 4;
  const int16_t stat2Y = statusTop + 3 * CARD_STATUS_H / 4;

  _tft->setFreeFont(F_SUB);
  _tft->setTextDatum(ML_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_SUB, UserConfiguration::COLOR_BACKGROUND);

  time_t now = time(nullptr);
  const bool hasTiming = (now > 1000000000L && f.actual_out_epoch > 0);

  if (hasTiming)
  {
    String dep = buildDepartedLine(f, now);
    String arr = buildArrivingLine(f, now);
    if (dep.length()) _tft->drawString(fitText(dep, _w - hPad * 2), hPad, stat1Y);
    if (arr.length()) _tft->drawString(fitText(arr, _w - hPad * 2), hPad, stat2Y);
    drawProgressBar(f, now);
  }
  else
  {
    String live   = fitText(resolveLiveSummary(f),   _w - hPad * 2);
    String motion = fitText(resolveMotionSummary(f), _w - hPad * 2);
    if (live.length())   _tft->drawString(live,   hPad, stat1Y);
    if (motion.length()) _tft->drawString(motion, hPad, stat2Y);
  }

  _tft->endWrite();

  // ── Phase 2: JPEG logo — drawn after endWrite() to avoid SPI nesting ───────
  if (hasLogo)
    TJpgDec.drawFsJpg(logoX, logoY, f.logo_path.c_str(), LittleFS);
}

void CYDDisplay::readRectRGB(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t *buf)
{
  if (_tft) _tft->readRectRGB(x, y, w, h, buf);
}

// ── Fetch status bar ─────────────────────────────────────────────────────────
// Overlays a thin bar at the very bottom of the screen while a fetch is active.
// Empty phase removes the bar and invalidates the render guard so the next
// displayFlights() call redraws cleanly.
void CYDDisplay::showFetchStatus(const char *phase)
{
  if (!_tft) return;
  const String p = (phase && phase[0] != '\0') ? String(phase) : String();
  if (_lastFetchPhase == p) return;
  _lastFetchPhase = p;

  const int16_t barH = CARD_BAR_H * 2;
  const int16_t barY = _h - barH;

  _tft->startWrite();
  if (p.length())
  {
    _tft->fillRect(0, barY, _w, barH, UserConfiguration::COLOR_HEADER_BG);
    _tft->drawFastHLine(0, barY, _w, UserConfiguration::COLOR_ROUTE);
    _tft->setFreeFont(F_SUB);
    _tft->setTextDatum(MC_DATUM);
    _tft->setTextColor(UserConfiguration::COLOR_ROUTE, UserConfiguration::COLOR_HEADER_BG);
    _tft->drawString(p, _w / 2, barY + barH / 2 + 1);
  }
  else
  {
    _tft->fillRect(0, barY, _w, barH, UserConfiguration::COLOR_BACKGROUND);
    resetRenderState(); // force full redraw of next flight card
  }
  _tft->endWrite();
}
