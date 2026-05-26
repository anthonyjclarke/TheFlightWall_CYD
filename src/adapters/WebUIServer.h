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
  void begin(const std::vector<FlightInfo> *flights, const CYDDisplay *display);
  void handle();
  void recordFetch(const std::vector<StateVector> &states,
                   const std::vector<FlightInfo> &flights,
                   size_t enriched);

  // Returns true once after a successful POST /api/config.
  // Caller is responsible for rebooting after a short flush delay.
  bool shouldReboot() const { return _pendingReboot; }

private:
  static constexpr size_t EVENT_CAPACITY = 50;

  WebServer _server{80};
  bool      _pendingReboot = false;
  const std::vector<FlightInfo> *_flights = nullptr;
  const CYDDisplay              *_display = nullptr;
  String _events[EVENT_CAPACITY];
  size_t _eventStart = 0;
  size_t _eventCount = 0;
  time_t _lastFetchEpoch = 0;

  void onRoot();
  void onGetConfig();
  void onPostConfig();
  void onGetLive();
  void onGetLogo();
  void onNotFound();
  void appendEvent(const String &message);
};
