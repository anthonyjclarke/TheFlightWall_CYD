#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <vector>
#include "FlightInfo.h"
#include "StateVector.h"

class CYDDisplay;

// HTTP server on port 80 providing live dashboard and runtime configuration.
// Routes:
//   GET  /            Embedded dashboard application
//   GET  /api/config  Current config as JSON
//   POST /api/config  Update config from JSON body; saves to NVS then triggers
//                     a millis()-based reboot (check shouldReboot() in loop).
//   GET  /api/live    Current TFT card, enriched flights, and volatile events
//   GET  /api/logo    Cached airline logo JPEG for dashboard rendering
class WebUIServer
{
public:
  void begin(const std::vector<FlightInfo> *flights, CYDDisplay *display);
  void handle();
  void recordFetch(const std::vector<StateVector> &states,
                   const std::vector<FlightInfo> &flights,
                   size_t enriched);
  void setApiError(const String &msg) { _apiError = msg; }
  void setCreditsRemaining(int n);  // called each fetch cycle after OpenSky returns

  // Call between individual API sub-calls to keep the WebServer responsive mid-fetch.
  void pump() { _server.handleClient(); }

  // Mark the device busy (and optionally name the current phase) so /api/live can
  // report it to the dashboard.  phase is a short label: "OpenSky", "AeroAPI 3/8", etc.
  void setBusy(bool b, const char *phase = "") {
    _busy = b;
    strncpy(_phase, phase ? phase : "", sizeof(_phase) - 1);
    _phase[sizeof(_phase) - 1] = '\0';
  }

  // Returns true once after a successful POST /api/config.
  // Caller is responsible for rebooting after a short flush delay.
  bool shouldReboot() const { return _pendingReboot; }

private:
  static constexpr size_t EVENT_CAPACITY = 50;

  WebServer _server{80};
  bool      _pendingReboot = false;
  const std::vector<FlightInfo> *_flights = nullptr;
  CYDDisplay                    *_display = nullptr;
  String _events[EVENT_CAPACITY];
  String _apiError;
  int    _creditsRemaining  = -1; // -1 = not yet observed
  bool   _creditsWarned     = false;
  bool   _busy              = false;
  char   _phase[24]         = {0};  // short label: "OpenSky", "AeroAPI 3/8", etc.
  size_t _eventStart = 0;
  size_t _eventCount = 0;
  time_t        _lastFetchEpoch = 0;
  unsigned long _lastFetchMs    = 0;  // millis() at last recordFetch; drives next_fetch_in countdown

  void onRoot();
  void onGetConfig();
  void onPostConfig();
  void onGetLive();
  void onGetLogo();
  void onFetchMap();
  void onGetMapPreview();
  void onGetScreenshot();
  void onNotFound();
  void appendEvent(const String &message);
};
