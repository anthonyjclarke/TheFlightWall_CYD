#include "adapters/CYDDisplay.h"

#include <TFT_eSPI.h>
#include "config/UserConfiguration.h"
#include "config/HardwareConfiguration.h"
#include "config/TimingConfiguration.h"
#include "debug.h"

// Font and layout constants differ between the two supported display variants.
// TFT_HEIGHT is the portrait-mode height (longer dimension):
//   ILI9341 → TFT_HEIGHT=320   ST7796 → TFT_HEIGHT=480
#if TFT_HEIGHT >= 400
#define F_ROUTE   &FreeSansBold24pt7b
#define F_AIRLINE &FreeSansBold12pt7b
#define F_SUB     &FreeSans9pt7b
static constexpr int HEADER_H = 44;
static constexpr int FOOTER_H = 36;
static constexpr bool SHOW_TWO_METRIC_LINES = true;
#else
#define F_ROUTE   &FreeSansBold18pt7b
#define F_AIRLINE &FreeSans9pt7b
#define F_SUB     &FreeSans9pt7b
static constexpr int HEADER_H = 28;
static constexpr int FOOTER_H = 26;
static constexpr bool SHOW_TWO_METRIC_LINES = false;
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
  const unsigned long intervalMs = TimingConfiguration::DISPLAY_CYCLE_SECONDS * 1000UL;

  if (flights.size() > 1 && now - _lastCycleMs >= intervalMs)
  {
    _lastCycleMs        = now;
    _currentFlightIndex = (_currentFlightIndex + 1) % flights.size();
  }

  const size_t idx = _currentFlightIndex % flights.size();
  const String key = renderKey(flights[idx]);
  if (_lastRenderedIndex == idx &&
      _lastRenderedTotal == flights.size() &&
      _lastRenderedKey == key)
  {
    return;
  }

  drawFlightCard(flights[idx], idx + 1, flights.size());
  _lastRenderedIndex = idx;
  _lastRenderedTotal = flights.size();
  _lastRenderedKey   = key;
}

void CYDDisplay::displayMessage(const String &message)
{
  if (!_tft)
    return;

  _tft->startWrite();
  _tft->fillScreen(UserConfiguration::COLOR_BACKGROUND);
  _tft->setFreeFont(F_SUB);
  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_MESSAGE, UserConfiguration::COLOR_BACKGROUND);
  _tft->drawString(message, _w / 2, _h / 2);
  _tft->endWrite();
  resetRenderState();
}

void CYDDisplay::showLoading()
{
  if (!_tft)
    return;

  _tft->startWrite();
  _tft->fillScreen(UserConfiguration::COLOR_BACKGROUND);
  _tft->setFreeFont(F_SUB);
  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_SUB, UserConfiguration::COLOR_BACKGROUND);
  _tft->drawString("Searching...", _w / 2, _h / 2);
  _tft->endWrite();
  resetRenderState();
}

String CYDDisplay::renderKey(const FlightInfo &f) const
{
  return resolveCallsign(f) + "|" +
         f.origin.code_icao + "|" +
         f.destination.code_icao + "|" +
         resolveAirline(f) + "|" +
         resolveAircraft(f) + "|" +
         resolveLiveSummary(f) + "|" +
         resolveMotionSummary(f);
}

void CYDDisplay::resetRenderState()
{
  _lastRenderedIndex = (size_t)-1;
  _lastRenderedTotal = 0;
  _lastRenderedKey   = "";
}

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
  {
    return "FL" + String((int)lround(feet / 100.0));
  }
  return String((int)lround(feet / 100.0) * 100) + "ft";
}

String CYDDisplay::resolveLiveSummary(const FlightInfo &f) const
{
  String summary;

  if (!isnan(f.distance_km))
  {
    summary += String((int)lround(f.distance_km));
    summary += "km";
    const char *dir = bearingCardinal(f.bearing_deg);
    if (dir[0])
    {
      summary += " ";
      summary += dir;
    }
  }

  String altitude = formatAltitude(f.baro_altitude_m, f.geo_altitude_m, f.on_ground);
  if (altitude.length())
  {
    if (summary.length())
      summary += "  ";
    summary += altitude;
  }

  return summary;
}

String CYDDisplay::resolveMotionSummary(const FlightInfo &f) const
{
  String summary;

  if (!isnan(f.velocity_mps))
  {
    summary += String((int)lround(f.velocity_mps * 3.6));
    summary += "km/h";
  }

  if (!isnan(f.vertical_rate_mps))
  {
    const double absRate = fabs(f.vertical_rate_mps);
    if (absRate >= 0.5)
    {
      if (summary.length())
        summary += " ";
      summary += f.vertical_rate_mps > 0 ? "UP" : "DN";
    }
  }

  if (!isnan(f.heading_deg))
  {
    if (summary.length())
      summary += " ";
    summary += String((int)lround(f.heading_deg));
    summary += "deg";
  }

  return summary;
}

// Truncate text with ".." until it fits within maxWidth pixels at the currently-set font.
String CYDDisplay::fitText(const String &text, int maxWidth)
{
  if (_tft->textWidth(text) <= maxWidth)
    return text;

  String t = text;
  while (t.length() > 1 && _tft->textWidth(t + "..") > maxWidth)
    t.remove(t.length() - 1);

  return t + "..";
}

// Card layout (320×240 example):
//   [0 .. HEADER_H-1]              airline name (left) + "n/m" counter (right)
//   [HEADER_H]                     1-px divider
//   [HEADER_H+1 .. h-FOOTER_H-2]   route "YSSY  >  YMML" centred, large font
//   [h-FOOTER_H-1]                 1-px divider
//   [h-FOOTER_H .. h-1]            callsign (left)  aircraft (right)
void CYDDisplay::drawFlightCard(const FlightInfo &f, size_t idx, size_t total)
{
  _tft->startWrite();

  _tft->fillScreen(UserConfiguration::COLOR_BACKGROUND);

  const int16_t dividerY = HEADER_H;
  const int16_t footerY  = _h - FOOTER_H;

  // Header bar
  _tft->fillRect(0, 0, _w, HEADER_H, UserConfiguration::COLOR_HEADER_BG);

  _tft->setFreeFont(F_AIRLINE);
  const int hMidY = HEADER_H / 2;
  const int hPad  = 6;

  // Reserve right-side space for counter before truncating airline name
  _tft->setTextDatum(ML_DATUM);
  const int counterW = _tft->textWidth("00/00") + hPad * 2;
  String airline = fitText(resolveAirline(f), _w - counterW - hPad * 2);
  _tft->setTextColor(UserConfiguration::COLOR_AIRLINE, UserConfiguration::COLOR_HEADER_BG);
  _tft->drawString(airline, hPad, hMidY);

  String counter = String(idx) + "/" + String(total);
  _tft->setTextDatum(MR_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_SUB, UserConfiguration::COLOR_HEADER_BG);
  _tft->drawString(counter, _w - hPad, hMidY);

  // Dividers
  _tft->drawFastHLine(0, dividerY,     _w, UserConfiguration::COLOR_DIVIDER);
  _tft->drawFastHLine(0, footerY - 1,  _w, UserConfiguration::COLOR_DIVIDER);

  // Route and live metrics — centred in the space between header and footer.
  const int contentTop = dividerY + 1;
  const int contentBottom = footerY - 2;
  const int contentMidY = contentTop + (contentBottom - contentTop) / 2;
  String origin = f.origin.code_icao.length()      ? f.origin.code_icao      : "----";
  String dest   = f.destination.code_icao.length() ? f.destination.code_icao : "----";

  _tft->setFreeFont(F_ROUTE);
  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_ROUTE, UserConfiguration::COLOR_BACKGROUND);
  _tft->drawString(origin + "  >  " + dest, _w / 2, contentMidY - 14);

  _tft->setFreeFont(F_SUB);
  _tft->setTextColor(UserConfiguration::COLOR_SUB, UserConfiguration::COLOR_BACKGROUND);
  String liveSummary = fitText(resolveLiveSummary(f), _w - hPad * 2);
  String motionSummary = fitText(resolveMotionSummary(f), _w - hPad * 2);
  if (SHOW_TWO_METRIC_LINES)
  {
    if (liveSummary.length())
      _tft->drawString(liveSummary, _w / 2, contentMidY + 18);
    if (motionSummary.length())
      _tft->drawString(motionSummary, _w / 2, contentMidY + 38);
  }
  else
  {
    String compactSummary = liveSummary;
    if (motionSummary.length())
    {
      if (compactSummary.length())
        compactSummary += "  ";
      compactSummary += motionSummary;
    }
    compactSummary = fitText(compactSummary, _w - hPad * 2);
    if (compactSummary.length())
      _tft->drawString(compactSummary, _w / 2, contentMidY + 18);
  }

  // Footer
  _tft->setFreeFont(F_SUB);
  const int fMidY = footerY + FOOTER_H / 2;

  _tft->setTextDatum(ML_DATUM);
  _tft->setTextColor(UserConfiguration::COLOR_SUB, UserConfiguration::COLOR_BACKGROUND);
  _tft->drawString(resolveCallsign(f), hPad, fMidY);

  String aircraft = resolveAircraft(f);
  if (aircraft.length())
  {
    _tft->setTextDatum(MR_DATUM);
    _tft->drawString(aircraft, _w - hPad, fMidY);
  }

  _tft->endWrite();
}
