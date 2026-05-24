#include "adapters/WebUIServer.h"

#include <ArduinoJson.h>
#include "config/RuntimeConfig.h"
#include "debug.h"

// ── Embedded HTML page ────────────────────────────────────────────────────────
// Single-file page; no external dependencies.  JS uses fetch() to GET/POST
// /api/config.  Dark amber theme matches the display aesthetic.
static const char HTML_PAGE[] PROGMEM = R"rawlit(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FlightWall Config</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#0d0d0d;color:#ddd;
  padding:18px 16px 32px;max-width:540px;margin:0 auto}
h1{color:#fd8200;margin-bottom:22px;font-size:1.35em;letter-spacing:.03em}
h2{color:#777;font-size:0.78em;letter-spacing:.13em;text-transform:uppercase;
  margin:24px 0 8px;border-bottom:1px solid #1e1e1e;padding-bottom:5px}
label{display:block;font-size:0.79em;color:#666;margin:10px 0 3px}
input[type=text],input[type=password],input[type=number]{
  width:100%;padding:7px 10px;background:#171717;border:1px solid #2a2a2a;
  border-radius:4px;color:#eee;font-size:0.93em}
input:focus{outline:none;border-color:#fd8200}
input[type=range]{width:100%;padding:0;border:none;background:none;
  accent-color:#fd8200;margin-top:4px}
.row{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.hint{font-size:0.72em;color:#3a3a3a;margin-top:3px}
#bval{color:#fd8200;font-weight:700}
button{display:block;width:100%;margin-top:24px;padding:11px;
  background:#fd8200;border:none;border-radius:4px;color:#000;
  font-size:0.97em;font-weight:700;cursor:pointer;letter-spacing:.04em}
button:hover{background:#c86600}
button:disabled{background:#5a3000;color:#333;cursor:default}
#msg{margin-top:13px;padding:9px 12px;border-radius:4px;text-align:center;
  font-size:0.87em;display:none}
</style>
</head>
<body>
<h1>&#9992;&nbsp;FlightWall Config</h1>

<h2>Location</h2>
<div class="row">
  <div>
    <label>Latitude</label>
    <input type="number" id="lat" step="0.0001" placeholder="-33.8688">
  </div>
  <div>
    <label>Longitude</label>
    <input type="number" id="lon" step="0.0001" placeholder="151.2093">
  </div>
</div>
<label>Search Radius (km)</label>
<input type="number" id="radius" min="1" max="500" step="1" placeholder="50">
<div class="hint">Bounding-box radius around the configured centre point.</div>

<h2>API Keys</h2>
<label>OpenSky Client ID</label>
<input type="text" id="osky_id" autocomplete="off" spellcheck="false">
<label>OpenSky Client Secret</label>
<input type="password" id="osky_sec" autocomplete="new-password">
<label>AeroAPI Key</label>
<input type="password" id="aero_key" autocomplete="new-password">
<div class="hint">Credentials are stored in device NVS and never logged.</div>

<h2>Timing &amp; Display</h2>
<div class="row">
  <div>
    <label>Fetch Interval (s)</label>
    <input type="number" id="fetch_sec" min="10" max="3600">
    <div class="hint">OpenSky poll rate &mdash; min 10 s.</div>
  </div>
  <div>
    <label>Card Cycle (s)</label>
    <input type="number" id="cycle_sec" min="1" max="60">
    <div class="hint">Seconds per flight card.</div>
  </div>
</div>
<label>Backlight brightness: <span id="bval">200</span></label>
<input type="range" id="brightness" min="0" max="255"
  oninput="document.getElementById('bval').textContent=this.value">

<button id="savebtn" onclick="saveConfig()">Save &amp; Reboot</button>
<div id="msg"></div>

<script>
function el(id){return document.getElementById(id);}

async function loadConfig(){
  try{
    const d=await(await fetch('/api/config')).json();
    el('lat').value=d.lat??'';
    el('lon').value=d.lon??'';
    el('radius').value=d.radius_km??50;
    el('osky_id').value=d.osky_id??'';
    el('osky_sec').value=d.osky_sec??'';
    el('aero_key').value=d.aero_key??'';
    el('fetch_sec').value=d.fetch_sec??30;
    el('cycle_sec').value=d.cycle_sec??3;
    const b=d.brightness??200;
    el('brightness').value=b;
    el('bval').textContent=b;
  }catch(e){showMsg('Could not load config: '+e,'#4a1a1a');}
}

async function saveConfig(){
  const btn=el('savebtn');
  btn.disabled=true;
  btn.textContent='Saving…';
  const payload={
    lat:parseFloat(el('lat').value)||0,
    lon:parseFloat(el('lon').value)||0,
    radius_km:Math.max(1,parseFloat(el('radius').value)||50),
    osky_id:el('osky_id').value,
    osky_sec:el('osky_sec').value,
    aero_key:el('aero_key').value,
    fetch_sec:Math.max(10,parseInt(el('fetch_sec').value)||30),
    cycle_sec:Math.max(1,parseInt(el('cycle_sec').value)||3),
    brightness:parseInt(el('brightness').value)
  };
  try{
    const r=await fetch('/api/config',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify(payload)
    });
    if(r.ok){
      showMsg('Saved — device rebooting…','#0f2a0f');
    } else {
      showMsg('Save failed (HTTP '+r.status+')','#4a1a1a');
      btn.disabled=false;btn.textContent='Save & Reboot';
    }
  }catch(e){
    showMsg('Save error: '+e,'#4a1a1a');
    btn.disabled=false;btn.textContent='Save & Reboot';
  }
}

function showMsg(text,bg){
  const m=el('msg');m.textContent=text;m.style.background=bg;m.style.display='block';
}

window.onload=loadConfig;
</script>
</body>
</html>
)rawlit";

// ── Server implementation ─────────────────────────────────────────────────────

void WebUIServer::begin()
{
  _server.on("/",           HTTP_GET,  [this] { onRoot(); });
  _server.on("/api/config", HTTP_GET,  [this] { onGetConfig(); });
  _server.on("/api/config", HTTP_POST, [this] { onPostConfig(); });
  _server.onNotFound(       [this] { onNotFound(); });
  _server.begin();
  DBG_INFO("WebUI: HTTP server listening on port 80");
}

void WebUIServer::handle()
{
  _server.handleClient();
}

void WebUIServer::onRoot()
{
  _server.sendHeader("Cache-Control", "no-cache, no-store");
  _server.send_P(200, "text/html", HTML_PAGE);
}

void WebUIServer::onGetConfig()
{
  DynamicJsonDocument doc(512);
  doc["lat"]        = RuntimeConfig::centerLat();
  doc["lon"]        = RuntimeConfig::centerLon();
  doc["radius_km"]  = RuntimeConfig::radiusKm();
  doc["fetch_sec"]  = RuntimeConfig::fetchIntervalSec();
  doc["cycle_sec"]  = RuntimeConfig::displayCycleSec();
  doc["brightness"] = RuntimeConfig::brightness();
  doc["osky_id"]    = RuntimeConfig::openskyClientId();
  doc["osky_sec"]   = RuntimeConfig::openskyClientSecret();
  doc["aero_key"]   = RuntimeConfig::aeroApiKey();

  String out;
  serializeJson(doc, out);
  _server.sendHeader("Cache-Control", "no-cache");
  _server.send(200, "application/json", out);
}

void WebUIServer::onPostConfig()
{
  String body = _server.arg("plain");
  if (body.isEmpty())
  {
    _server.send(400, "application/json", "{\"error\":\"empty body\"}");
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, body);
  if (err)
  {
    DBG_WARN("WebUI: POST JSON parse error: %s", err.c_str());
    _server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }

  if (doc.containsKey("lat"))        RuntimeConfig::setCenterLat(doc["lat"].as<double>());
  if (doc.containsKey("lon"))        RuntimeConfig::setCenterLon(doc["lon"].as<double>());
  if (doc.containsKey("radius_km"))  RuntimeConfig::setRadiusKm(doc["radius_km"].as<double>());
  if (doc.containsKey("fetch_sec"))  RuntimeConfig::setFetchIntervalSec(doc["fetch_sec"].as<uint32_t>());
  if (doc.containsKey("cycle_sec"))  RuntimeConfig::setDisplayCycleSec(doc["cycle_sec"].as<uint32_t>());
  if (doc.containsKey("brightness")) RuntimeConfig::setBrightness(doc["brightness"].as<uint8_t>());
  if (doc.containsKey("osky_id"))    RuntimeConfig::setOpenskyClientId(doc["osky_id"].as<String>());
  if (doc.containsKey("osky_sec"))   RuntimeConfig::setOpenskyClientSecret(doc["osky_sec"].as<String>());
  if (doc.containsKey("aero_key"))   RuntimeConfig::setAeroApiKey(doc["aero_key"].as<String>());

  RuntimeConfig::save();

  _server.send(200, "application/json", "{\"ok\":true}");
  DBG_INFO("WebUI: config saved — reboot scheduled");
  _pendingReboot = true;
}

void WebUIServer::onNotFound()
{
  _server.send(404, "text/plain", "Not found");
}
