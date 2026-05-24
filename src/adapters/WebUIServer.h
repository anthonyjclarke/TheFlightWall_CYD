#pragma once

#include <Arduino.h>
#include <WebServer.h>

// HTTP server on port 80 providing a configuration WebUI.
// Routes:
//   GET  /            HTML configuration page
//   GET  /api/config  Current config as JSON
//   POST /api/config  Update config from JSON body; saves to NVS then triggers
//                     a millis()-based reboot (check shouldReboot() in loop).
class WebUIServer
{
public:
  void begin();
  void handle();

  // Returns true once after a successful POST /api/config.
  // Caller is responsible for rebooting after a short flush delay.
  bool shouldReboot() const { return _pendingReboot; }

private:
  WebServer _server{80};
  bool      _pendingReboot = false;

  void onRoot();
  void onGetConfig();
  void onPostConfig();
  void onNotFound();
};
