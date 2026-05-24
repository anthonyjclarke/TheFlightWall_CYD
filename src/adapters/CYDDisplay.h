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

private:
    TFT_eSPI *_tft = nullptr;
    uint16_t  _w   = 0;
    uint16_t  _h   = 0;

    size_t        _currentFlightIndex = 0;
    unsigned long _lastCycleMs        = 0;
    size_t        _lastRenderedIndex  = (size_t)-1;
    size_t        _lastRenderedTotal  = 0;
    String        _lastRenderedKey;

    void drawFlightCard(const FlightInfo &f, size_t idx, size_t total);
    void drawProgressBar(const FlightInfo &f, time_t now);
    String renderKey(const FlightInfo &f) const;
    void resetRenderState();

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

    String fitText(const String &text, int maxWidth);
};
