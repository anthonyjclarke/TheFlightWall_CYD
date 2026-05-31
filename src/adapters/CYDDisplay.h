#pragma once

#include <stdint.h>
#include <time.h>
#include <vector>
#include "BaseDisplay.h"

class TFT_eSPI;

class CYDDisplay : public BaseDisplay
{
public:
    CYDDisplay();
    ~CYDDisplay() override;

    bool initialize()                                    override;
    void clear()                                         override;
    void displayFlights(const std::vector<FlightInfo> &) override;
    void displayMessage(const String &message)          override;
    void showLoading()                                   override;
    void showFetchStatus(const char *phase);

    // Call before switching display content (clear, message, loading) so the
    // next displayFlights() call re-renders even if the slot key is unchanged.
    void resetRenderState();

    // Draws /splash.jpg from LittleFS at (0,0). Returns false if the file
    // is missing or decode fails — caller may then fall back to a text
    // banner. Re-uses the TJpgDec setup done in initialize().
    bool showSplash();

    size_t currentFlightIndex() const { return _currentFlightIndex; }
    uint16_t width() const { return _w; }
    uint16_t height() const { return _h; }

    // Set backlight brightness (0–255) at runtime via the LEDC channel.
    void setBrightness(uint8_t level);

    // Pass-through to TFT_eSPI::readRectRGB — reads a rectangle of pixels from the
    // live framebuffer as packed RGB888 (3 bytes per pixel, R/G/B order). On ILI9341
    // the low 2 bits of each channel are zero (the chip stores 18-bit colour).
    // Buffer must be at least w * h * 3 bytes. Kept here to avoid leaking the TFT_eSPI
    // pointer to other adapters.
    void readRectRGB(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t *buf);

    // Force one map-card render regardless of flight list state.
    // Used by main.cpp to honour WebUI "Preview Map" requests.
    void showStandaloneMap();

private:
    TFT_eSPI *_tft = nullptr;
    uint16_t  _w   = 0;
    uint16_t  _h   = 0;

    size_t        _currentFlightIndex = 0;
    unsigned long _lastCycleMs        = 0;
    size_t        _lastRenderedIndex  = (size_t)-1;
    size_t        _lastRenderedTotal  = 0;
    String        _lastRenderedKey;
    String        _lastStatusMessage; // tracks displayMessage / showLoading text to avoid flicker
    String        _lastFetchPhase;   // tracks fetch status bar to avoid redundant redraws
    bool          _splashOnScreen = false; // splash idempotence guard — set by showSplash(), cleared by every full-screen draw

    void drawFlightCard(const FlightInfo &f, size_t idx, size_t total);
    void drawProgressBar(const FlightInfo &f, time_t now);
    String renderKey(const FlightInfo &f) const;

    // Resolve helpers
    String resolveAirline(const FlightInfo &f) const;
    String resolveCallsign(const FlightInfo &f) const;
    String resolveAircraft(const FlightInfo &f) const;
    String resolveIataOrIcao(const AirportInfo &ap) const;
    String resolveCityOrCode(const AirportInfo &ap) const;
    String resolveLiveSummary(const FlightInfo &f) const;
    String resolveMotionSummary(const FlightInfo &f) const;

    // Status line builders
    String buildDepartedLine(const FlightInfo &f, time_t now) const;
    String buildArrivingLine(const FlightInfo &f, time_t now) const;

    // Radar map card
    void   drawMapCard(const std::vector<FlightInfo> &flights);
    String mapRenderKey(const std::vector<FlightInfo> &flights) const;

    String fitText(const String &text, int maxWidth);
};
