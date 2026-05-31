#include "WebUIServer.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <math.h>
#include <time.h>
#include "CYDDisplay.h"
#include "RuntimeConfig.h"
#include "Version.h"
#include "debug.h"

// Single-file dashboard application. Data is polled from the device so the
// ESP32 does not need WebSocket buffering or any persistent log storage.
static const char HTML_PAGE[] PROGMEM = R"rawlit(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>The Flight Wall &#8212; CYD v)rawlit" FW_VERSION_STR R"rawlit(</title>
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCA2NCA2NCIgd2lkdGg9IjY0IiBoZWlnaHQ9IjY0IiByb2xlPSJpbWciIGFyaWEtbGFiZWw9IlRoZSBGbGlnaHQgV2FsbCI+CiAgCiAgPHJlY3Qgd2lkdGg9IjY0IiBoZWlnaHQ9IjY0IiByeD0iMTIiIHJ5PSIxMiIgZmlsbD0iIzA2MDkwZSI+PC9yZWN0PgogIAogIDxjaXJjbGUgY3g9IjMyIiBjeT0iMzIiIHI9IjIyIiBmaWxsPSJub25lIiBzdHJva2U9IiNmZjliMmUiIHN0cm9rZS1vcGFjaXR5PSIwLjQ1IiBzdHJva2Utd2lkdGg9IjEuNSI+PC9jaXJjbGU+CiAgPGNpcmNsZSBjeD0iMzIiIGN5PSIzMiIgcj0iMTMiIGZpbGw9Im5vbmUiIHN0cm9rZT0iI2ZmOWIyZSIgc3Ryb2tlLW9wYWNpdHk9IjAuNzUiIHN0cm9rZS13aWR0aD0iMS41Ij48L2NpcmNsZT4KICAKICA8bGluZSB4MT0iMTAiIHkxPSIzMiIgeDI9IjU0IiB5Mj0iMzIiIHN0cm9rZT0iI2ZmOWIyZSIgc3Ryb2tlLW9wYWNpdHk9IjAuMiIgc3Ryb2tlLXdpZHRoPSIxIj48L2xpbmU+CiAgPGxpbmUgeDE9IjMyIiB5MT0iMTAiIHgyPSIzMiIgeTI9IjU0IiBzdHJva2U9IiNmZjliMmUiIHN0cm9rZS1vcGFjaXR5PSIwLjIiIHN0cm9rZS13aWR0aD0iMSI+PC9saW5lPgogIAogIDxwYXRoIGQ9Ik0gMzIgMzIgTCAzMiAxMCBBIDIyIDIyIDAgMCAxIDUxLjA1IDIxIFoiIGZpbGw9IiNmZjliMmUiIG9wYWNpdHk9IjAuNiI+PC9wYXRoPgogIAogIDxjaXJjbGUgY3g9IjMyIiBjeT0iMzIiIHI9IjIuNSIgZmlsbD0iI2ZmOWIyZSI+PC9jaXJjbGU+Cjwvc3ZnPg==">
<style>
:root{--bg:#06090e;--panel:#0c1218;--panel2:#111a21;--line:#1d2b34;--ink-dim:#8a8f99;--ink:#f3f4f6;--amber:#ff9b2e;--green:#3ddc7a;--cyan:#5fb7d6;--red:#e87063;--glow:rgba(255,155,46,.17);--font-display:"Space Grotesk",system-ui,sans-serif;--font-mono:"JetBrains Mono",ui-monospace,SFMono-Regular,Consolas,monospace}
/* Inline-logo SVG text styling — SVG fills inherit from these via the .wordmark/.subline classes */
.wordmark{font:600 22px/1 var(--font-display);fill:var(--ink)}
.subline{font:500 10px/1 var(--font-display);fill:var(--amber);letter-spacing:.18em}
*{box-sizing:border-box}html,body{margin:0;min-height:100%;background:var(--bg);color:var(--ink);font-family:var(--font-display)}
body{background-image:radial-gradient(circle at 18% 0,rgba(255,155,46,.08),transparent 32rem),linear-gradient(135deg,#06090e,#080f13 62%,#06090e)}
.shell{max-width:1380px;margin:auto;padding:24px}
header{display:flex;align-items:flex-end;justify-content:space-between;gap:18px;margin-bottom:22px}
.eyebrow{display:block;color:var(--amber);font-size:11px;letter-spacing:.22em;text-transform:uppercase;margin-bottom:9px}
h1{font-size:clamp(26px,3vw,38px);font-weight:540;line-height:1;margin:0;letter-spacing:-.04em}
.status{display:flex;align-items:center;gap:12px;border:1px solid var(--line);background:rgba(12,18,24,.75);border-radius:30px;padding:9px 15px;font-size:13px;color:var(--ink-dim)}
.dot{width:9px;height:9px;border-radius:50%;background:var(--green);box-shadow:0 0 12px var(--green)}
.api-alert{display:none;padding:7px 24px;font-size:13px;font-weight:500;background:rgba(232,112,99,.12);border-bottom:1px solid var(--red);color:var(--red)}
.busy-bar{display:none;padding:7px 24px;font-size:13px;font-weight:500;background:rgba(255,155,46,.10);border-bottom:1px solid var(--amber);color:var(--amber);align-items:center;gap:10px}
.busy-bar.show{display:flex}
.busy-dot{flex-shrink:0;width:8px;height:8px;border-radius:50%;background:var(--amber);animation:pulse 1.2s infinite ease-in-out}
@keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:.35;transform:scale(.7)}}
.grid{display:grid;grid-template-columns:minmax(350px,520px) minmax(350px,1fr);gap:18px;margin-bottom:18px}
.panel{background:linear-gradient(145deg,rgba(15,23,29,.94),rgba(8,13,18,.96));border:1px solid var(--line);border-radius:18px;box-shadow:0 16px 44px rgba(0,0,0,.22);overflow:hidden}
.panel-head{display:flex;justify-content:space-between;align-items:center;padding:18px 20px 12px}
.panel-title{font-size:12px;color:var(--ink-dim);letter-spacing:.16em;text-transform:uppercase}
.pill{border:1px solid #223641;padding:4px 10px;border-radius:20px;color:var(--cyan);font-size:11px}
.monitor-wrap{padding:10px 20px 23px}
.bezel{background:#121a20;border-radius:16px;padding:14px;box-shadow:inset 0 0 0 1px #29343c,0 22px 36px rgba(0,0,0,.35)}
.tft{width:100%;aspect-ratio:4/3;background:#000;border-radius:4px;display:flex;flex-direction:column;color:#fff;font-family:Arial,sans-serif;overflow:hidden}
.tft[data-wide=true]{aspect-ratio:3/2}
.tft-top{height:17%;display:flex;align-items:center;border-bottom:1px solid #272d30;padding:0 3%;gap:18px}
.tft-top.pin{background:var(--amber);border-bottom-color:var(--amber)}.tft-top.pin .tft-count,.tft-top.pin .tft-ident{color:#16181a}
.tft-count{font-size:clamp(11px,1vw,14px);color:#acb5ba}
.tft-ident{font-size:clamp(25px,3vw,38px);font-weight:700;flex:1;text-align:center;margin-right:36px}
.tft-mid{height:43%;border-bottom:1px solid #272d30;display:grid;grid-template-columns:37% 63%;align-items:center}
.tft-airline{text-align:center;color:#fff;font-weight:600;padding:8px;font-size:clamp(15px,1.5vw,22px)}
.tft-airline img{max-height:72px;max-width:92%;object-fit:contain;display:block;margin:auto}
.tft-route{text-align:center;color:#fd942d;font-size:clamp(25px,3vw,38px);font-weight:700}
.tft-aircraft{font-size:clamp(12px,1.25vw,17px);color:#c4cbd0;margin-top:8px}
.tft-status{flex:1;padding:5% 4% 0;color:#c8d0d5;font-size:clamp(12px,1.23vw,17px)}
.tft-status div{margin-bottom:7px}
.bar{height:5%;margin:0 3% 3%;background:#053820}.bar span{display:block;height:100%;background:#16b760;width:0}
.monitor-foot{display:flex;justify-content:space-between;color:var(--ink-dim);font-size:12px;padding-top:14px}
.stream{height:390px;overflow:auto;padding:4px 20px 20px;font-family:var(--font-mono);font-size:12px}
.event{padding:10px 0;border-bottom:1px solid rgba(29,43,52,.72);line-height:1.55;color:#c4ced4}
.event:first-child{color:#e7ecee}.empty{color:var(--ink-dim);padding:22px 0}
.section{margin-bottom:18px}.cards{display:grid;grid-auto-flow:column;grid-auto-columns:minmax(230px,280px);gap:12px;padding:0 18px 18px;overflow-x:auto;scroll-behavior:smooth;-webkit-overflow-scrolling:touch}.cards::-webkit-scrollbar{height:4px}.cards::-webkit-scrollbar-thumb{background:var(--line);border-radius:2px}.nav-btn{background:transparent;border:1px solid var(--line);color:var(--ink-dim);width:26px;height:26px;border-radius:6px;font-size:18px;line-height:1;cursor:pointer;padding:0;display:inline-flex;align-items:center;justify-content:center}.nav-btn:hover{border-color:var(--amber);color:var(--amber);background:rgba(255,155,46,.1)}.tag-adsb{color:var(--cyan);border-color:rgba(95,183,214,.32)}
.flight{background:var(--panel2);border:1px solid var(--line);border-radius:14px;padding:15px;min-height:250px}
.flight.pin{border-color:rgba(255,155,46,.55);box-shadow:0 0 0 1px rgba(255,155,46,.2),0 0 18px rgba(255,155,46,.08)}
.flight-id{display:flex;justify-content:space-between;align-items:start;margin-bottom:15px}.flight h3{font-size:22px;margin:0;letter-spacing:-.02em}
.tags{display:flex;gap:5px;flex-wrap:wrap;justify-content:flex-end}
.tag{color:var(--green);border:1px solid rgba(72,208,149,.32);font-size:10px;border-radius:12px;padding:3px 7px;letter-spacing:.1em}
.tag-pin{color:var(--amber);border-color:rgba(255,155,46,.5);background:rgba(255,155,46,.08)}
.route{font-size:22px;color:var(--amber);font-weight:650;margin-bottom:4px}.city{font-size:12px;color:var(--ink-dim);min-height:32px;margin-bottom:12px}
.facts{display:grid;grid-template-columns:1fr 1fr;gap:10px 8px}.fact span{display:block;color:var(--ink-dim);font-size:10px;text-transform:uppercase;letter-spacing:.11em;margin-bottom:3px}.fact b{font-size:13px;font-weight:500}
.settings{padding:0 20px 22px}.settings-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}.field label{display:block;color:var(--ink-dim);font-size:11px;letter-spacing:.1em;text-transform:uppercase;margin:0 0 6px}
input{width:100%;background:#0a1015;border:1px solid var(--line);border-radius:8px;padding:9px 10px;color:var(--ink);font-size:13px}input:focus{outline:none;border-color:var(--amber)}
.credentials{margin-top:16px;display:grid;grid-template-columns:repeat(3,1fr);gap:12px}.cred-note{font-size:11px;color:var(--ink-dim);margin-top:6px}.configured{color:var(--green)}
.info{display:inline-block;position:relative;color:var(--amber);cursor:help;font-size:13px;margin-left:4px;text-transform:none;letter-spacing:0}
.info::after{content:attr(data-tip);position:absolute;bottom:calc(100% + 7px);left:50%;transform:translateX(-50%);width:260px;background:#1d2b34;color:#e8edf0;font-size:11px;font-weight:400;line-height:1.5;text-transform:none;letter-spacing:0;padding:8px 10px;border-radius:7px;border:1px solid var(--line);white-space:normal;pointer-events:none;opacity:0;transition:opacity .15s;z-index:200}
.info:hover::after{opacity:1}
.actions{display:flex;justify-content:space-between;align-items:center;margin-top:18px;gap:10px}.message{font-size:13px;color:var(--ink-dim)}
button{background:var(--amber);border:0;border-radius:9px;padding:11px 18px;font-weight:650;cursor:pointer;color:#181005}button:disabled{opacity:.45;cursor:default}
@media(max-width:1120px){.settings-grid,.credentials{grid-template-columns:repeat(2,1fr)}}
@media(max-width:800px){.shell{padding:14px}.grid{grid-template-columns:1fr}.stream{height:280px}.settings-grid,.credentials{grid-template-columns:1fr}header{align-items:start;flex-direction:column}}
footer{margin-top:28px;padding:22px 0 8px;border-top:1px solid var(--line);display:grid;grid-template-columns:1fr 2fr;gap:24px;color:var(--ink-dim);font-size:12px}
footer h4{color:var(--ink);font-size:11px;letter-spacing:.15em;text-transform:uppercase;margin:0 0 8px}footer a{color:var(--cyan);text-decoration:none}footer a:hover{text-decoration:underline}
.ack{font-size:11px;color:var(--ink-dim);text-align:center;padding:12px 0 4px;border-top:1px solid var(--line);margin-top:16px}
@media(max-width:800px){footer{grid-template-columns:1fr}}
</style>
</head>
<body><div class="shell">
<header><div class="brand"><svg viewBox="0 0 360 80" xmlns="http://www.w3.org/2000/svg" aria-label="The Flight Wall &#8212; CYD Edition" style="height:54px;width:auto;display:block"><g transform="translate(18 9)"><circle cx="31" cy="31" r="27" fill="none" stroke="#ff9b2e" stroke-opacity=".45"/><circle cx="31" cy="31" r="16" fill="none" stroke="#ff9b2e" stroke-opacity=".7"/><line x1="4" y1="31" x2="58" y2="31" stroke="#ff9b2e" stroke-opacity=".18"/><line x1="31" y1="4" x2="31" y2="58" stroke="#ff9b2e" stroke-opacity=".18"/><path d="M31 31L31 4A27 27 0 0 1 54.38 17.5Z" fill="#ff9b2e" opacity=".55"/><circle cx="31" cy="31" r="2.5" fill="#ff9b2e"/></g><text class="wordmark" x="96" y="37">The Flight Wall</text><text class="subline" x="96" y="58">CYD EDITION</text></svg><small style="display:block;color:var(--ink-dim);font-size:11px;letter-spacing:.05em;margin-top:6px;padding-left:2px">v)rawlit" FW_VERSION_STR R"rawlit(</small></div><div class="status"><span class="dot"></span><span id="connection">Connecting to device</span><span id="clock"></span><span id="nextupd" style="display:none;padding-left:10px;border-left:1px solid var(--line);margin-left:4px"></span><span id="credits" style="display:none;padding-left:10px;border-left:1px solid var(--line);margin-left:4px"></span></div></header><div class="api-alert" id="api-alert"></div><div class="busy-bar" id="busy-bar"><span class="busy-dot"></span><span id="busy-text">Device busy</span></div>
<div class="grid">
 <section class="panel"><div class="panel-head"><span class="panel-title">TFT Mirror <span id="resolution" style="font-weight:400;opacity:.55;font-size:14px;letter-spacing:.01em;text-transform:none"></span></span><div style="display:flex;gap:8px;align-items:center"><span class="pill" id="map-pill" style="display:none;color:var(--amber);border-color:var(--amber)">&#9673; MAP CARD</span><span class="pill">Browser Replica / Low Impact</span></div></div><div class="monitor-wrap"><div class="bezel"><div class="tft" id="tft"><div class="tft-top" id="stoptop"><span class="tft-count" id="scount">0/0</span><span class="tft-ident" id="sident">SEARCHING...</span></div><div class="tft-mid"><div class="tft-airline" id="sairline"></div><div><div class="tft-route" id="sroute">--- - ---</div><div class="tft-aircraft" id="saircraft"></div></div></div><div class="tft-status"><div id="sline1"></div><div id="sline2"></div></div><div class="bar"><span id="sbar"></span></div></div><div id="tftmap" style="display:none;width:100%;aspect-ratio:4/3;background:#000;border-radius:4px;overflow:hidden;position:relative"><img id="tftmaptile" style="width:100%;height:100%;display:block;object-fit:cover" alt="Map mirror"><svg id="tftmapol" preserveAspectRatio="none" style="position:absolute;top:0;left:0;width:100%;height:100%;pointer-events:none"></svg></div></div><div class="monitor-foot"><a href="/api/screenshot" download style="color:var(--ink-dim);font-size:12px;text-decoration:none;border:1px solid var(--line);padding:4px 10px;border-radius:4px">&#8681; Screenshot (BMP)</a></div></div></section>
 <section class="panel"><div class="panel-head"><span class="panel-title">Flight Data Feed</span><span class="pill">Volatile / API Reads</span></div><div class="stream" id="events"><div class="empty">Waiting for a fetch cycle...</div></div></section>
</div>
<section class="panel section"><div class="panel-head"><span class="panel-title">Current Flights</span><div style="display:flex;align-items:center;gap:8px"><button class="nav-btn" onclick="scrollCards(-1)">&#8249;</button><span class="pill" id="flightCount">0 flights</span><button class="nav-btn" onclick="scrollCards(1)">&#8250;</button></div></div><div class="cards" id="flights"><div class="empty">No current flights.</div></div></section>
<section class="panel"><div class="panel-head"><span class="panel-title">Device Configuration</span><span class="pill">Protected Credentials</span></div><div class="settings">
 <div class="settings-grid">
  <div class="field"><label>Latitude</label><input type="number" id="lat" step="0.0001"></div><div class="field"><label>Longitude</label><input type="number" id="lon" step="0.0001"></div>
  <div class="field"><label>Radius km</label><input type="number" id="radius" min="1" max="500"></div><div class="field"><label>Brightness</label><input type="number" id="brightness" min="0" max="255"></div>
  <div class="field"><label>Fetch interval sec</label><input type="number" id="fetch_sec" min="10" max="3600"></div><div class="field"><label>Card cycle sec</label><input type="number" id="cycle_sec" min="1" max="60"></div><div class="field"><label>Map display sec</label><input type="number" id="map_sec" min="5" max="300"></div>
  <div class="field"><label>Map label colour <span class="info" data-tip="Colour used for enriched flight markers (dot, heading tick and callsign label) on BOTH the CYD map card and the WebUI map preview. Saved to NVS — applied live on the next map render.">&#9432;</span></label><input type="color" id="labelColor" value="#1e90ff" oninput="updateMapOverlay()" style="width:100%;height:38px;padding:2px;cursor:pointer"></div>
  <div class="field"><label>Pinned Flight <span class="info" data-tip="Card always at slot 1 in the TFT cycle, tracked every fetch regardless of radar radius. IATA (e.g. QF1) or ICAO (e.g. QFA001). Clear to disable.">&#9432;</span></label><input type="text" id="pinned_flight" placeholder="e.g. QF1 or QFA001 (Enter to save)" maxlength="8" style="text-transform:uppercase" onkeydown="if(event.key==='Enter'){event.preventDefault();savePinnedFlight();}"><div class="cred-note"><button onclick="savePinnedFlight()" style="background:transparent;border:1px solid var(--line);color:var(--ink-dim);font-size:11px;padding:3px 10px;border-radius:6px;cursor:pointer" onmouseover="this.style.borderColor='var(--amber)';this.style.color='var(--amber)'" onmouseout="this.style.borderColor='var(--line)';this.style.color='var(--ink-dim)'">Update</button> <span id="pinnedMsg" style="font-size:11px"></span></div></div>
 </div>
 <div id="mapwrap" style="margin:14px 0 0;display:none;border:1px solid var(--line);border-radius:12px;overflow:hidden;max-width:50%"><div style="display:flex;justify-content:space-between;align-items:center;padding:7px 14px;background:var(--panel2)"><span style="font-size:11px;color:var(--ink-dim);letter-spacing:.1em;text-transform:uppercase">Map Preview</span><div style="display:flex;align-items:center;gap:10px"><label style="display:flex;align-items:center;gap:5px;font-size:11px;color:var(--ink-dim);cursor:pointer;user-select:none"><input type="checkbox" id="showFlights" checked onchange="updateMapOverlay()" style="width:auto;margin:0;cursor:pointer"> Flights</label><button onclick="refreshMapPreview()" style="background:transparent;border:1px solid var(--line);color:var(--ink-dim);font-size:11px;padding:3px 10px;border-radius:6px;cursor:pointer">&#8635; Refresh</button></div></div><div style="position:relative;line-height:0"><img id="maptile" style="width:100%;display:block" alt="Map preview"><svg id="mapol" viewBox="0 0 320 240" style="position:absolute;top:0;left:0;width:100%;height:100%;pointer-events:none"></svg></div></div>
 <div class="credentials">
  <div class="field"><label>OpenSky Client ID <span id="oskyIdState"></span></label><input type="password" id="osky_id" placeholder="Leave blank to retain"><div class="cred-note"><input type="checkbox" id="clear_osky" style="width:auto"> Clear OpenSky credentials</div></div>
  <div class="field"><label>OpenSky Secret <span id="oskyState"></span></label><input type="password" id="osky_sec" placeholder="Leave blank to retain"></div>
  <div class="field"><label>AeroAPI Key <span id="aeroState"></span></label><input type="password" id="aero_key" placeholder="Leave blank to retain"><div class="cred-note"><input type="checkbox" id="clear_aero" style="width:auto"> Clear AeroAPI key</div></div>
 </div>
 <div class="actions"><span class="message" id="msg">Credential values are never returned to this page.</span><div style="display:flex;gap:10px"><button id="mapfetch" style="background:var(--panel2);border:1px solid var(--line);color:var(--ink-dim);font-weight:500;border-radius:9px;padding:11px 18px;cursor:pointer" onmouseover="this.style.borderColor='var(--amber)';this.style.color='var(--amber)'" onmouseout="this.style.borderColor='var(--line)';this.style.color='var(--ink-dim)'" onclick="fetchMap(this)">Fetch Map</button><button id="reboot" style="background:var(--panel2);border:1px solid var(--line);color:var(--ink-dim);font-weight:500;border-radius:9px;padding:11px 18px;cursor:pointer" onmouseover="this.style.borderColor='#e0533a';this.style.color='#e0533a'" onmouseout="this.style.borderColor='var(--line)';this.style.color='var(--ink-dim)'" onclick="rebootDevice()" title="Reboot the device — needed only for WiFi reset or recovery">Reboot Device</button><button id="save" onclick="saveConfig()">Save</button></div></div>
</div></section>
<footer>
<div><h4>Contact</h4><div>Anthony Clarke</div><div><a href="mailto:anthonyjclarke [at] gmail.com">anthonyjclarke [@] gmail.com</a></div><div style="margin-top:8px"><a href="https://github.com/anthonyjclarke">GitHub</a> &middot; <a href="https://bsky.app/profile/anthonyjclarke.bsky.social">BlueSky</a> &middot; <a href="https://www.threads.net/@anthonyjclarke">Threads</a> &middot; <a href="https://www.linkedin.com/in/anthonyjclarke">LinkedIn</a></div></div>
<div><h4>Origin</h4><div style="margin-bottom:12px;font-size:13px"><a href="https://github.com/AxisNimble/TheFlightWall_OSS" style="font-size:13px;font-weight:600">TheFlightWall OSS</a> by <a href="https://github.com/AxisNimble">AxisNimble</a> &#8212; the open-source flight wall project this firmware extends.</div><h4>Data Sources &amp; Libraries</h4><div style="display:grid;grid-template-columns:1fr 1fr;gap:6px 18px"><div><a href="https://opensky-network.org">OpenSky Network</a> &#8212; ADS-B state vectors</div><div><a href="https://flightaware.com/commercial/aeroapi">FlightAware AeroAPI</a> &#8212; flight enrichment</div><div><a href="https://github.com/Jxck-S/airline-logos">Jxck-S/airline-logos</a> &#8212; airline logo assets</div><div><a href="https://images.weserv.nl">images.weserv.nl</a> &#8212; image proxy &amp; resize</div><div><a href="https://github.com/Bodmer/TFT_eSPI">TFT_eSPI</a> by Bodmer &#8212; TFT display driver</div><div><a href="https://github.com/Bodmer/TJpg_Decoder">TJpg_Decoder</a> by Bodmer &#8212; JPEG rendering</div><div><a href="https://github.com/tzapu/WiFiManager">WiFiManager</a> by tzapu &#8212; WiFi provisioning</div><div><a href="https://arduinojson.org">ArduinoJson</a> by Beno&icirc;t Blanchon &#8212; JSON library</div></div></div>
</footer>
<div class="ack">The Flight Wall CYD Edition &#8212; open-source ESP32 flight radar. Built with Arduino &amp; PlatformIO.</div>
</div>
<script>
const $=id=>document.getElementById(id); let configLoaded=false; let g_mapFlights=[];
const val=(v,fallback='--')=>(v===undefined||v===null||v==='')?fallback:v;
const esc=v=>String(v).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function fixed(v,n=1){return (v===undefined||v===null)?'--':Number(v).toFixed(n)}
function when(epoch){if(!epoch)return '--';return new Date(epoch*1000).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});}
function route(f){return `${val(f.origin_iata,f.origin_icao||'---')} - ${val(f.destination_iata,f.destination_icao||'---')}`;}
function source(v){return ['ADS-B','ASTERIX','FLARM'][v]||val(v);}
function statusLines(f){
 const now=Date.now()/1000;
 if(f.actual_out_epoch){
  const ago=Math.max(0,Math.floor((now-f.actual_out_epoch)/60)), until=f.estimated_in_epoch?Math.floor((f.estimated_in_epoch-now)/60):null;
  const out=ago<60?`${ago} min ago`:`${Math.floor(ago/60)}h ${ago%60}m ago`;
  let arrive=''; if(until!==null) arrive=until<=0?'Arrived':(until<60?`Arriving in ${until} min`:`Arriving in ${Math.floor(until/60)}h ${until%60}m`);
  return [`Departed ${val(f.origin_city,f.origin_iata||'')} ${out}`,arrive];
 }
 const alt=f.on_ground?'GROUND':(f.baro_altitude_m!=null?`${Math.round(f.baro_altitude_m*3.28084/100)*100}ft`:'');
 return [`${fixed(f.distance_km)}km  ${alt}`,`${f.velocity_mps!=null?Math.round(f.velocity_mps*3.6)+'km/h':''}  ${f.heading_deg!=null?Math.round(f.heading_deg)+' deg':''}`];
}
function renderScreen(screen){
 $('tft').dataset.wide=screen.width>400; $('resolution').textContent=screen.width?`— ${screen.width} × ${screen.height} physical`:'';
 const isMap=screen.kind==='map';
 $('map-pill').style.display=isMap?'':'none';
 $('tft').style.display=isMap?'none':'';
 $('tftmap').style.display=isMap?'':'none';
 if(isMap){
  const tile=$('tftmaptile');
  if(!tile.src||tile.dataset.stale==='1'){tile.src='/api/mappreview?t='+Date.now();tile.dataset.stale='';}
  if(tile.complete&&tile.naturalWidth)updateMapOverlay(tile,$('tftmapol'));
  else tile.onload=function(){updateMapOverlay(tile,$('tftmapol'));};
  $('scount').textContent='MAP';$('stoptop').classList.remove('pin');
  return;
 }
 const f=screen.flight;
 if(!f){$('sident').textContent='SEARCHING...';$('scount').textContent='0/0';$('sroute').textContent='--- - ---';$('sairline').textContent='';$('saircraft').textContent='';$('sline1').textContent='';$('sline2').textContent='';$('sbar').style.width='0';$('stoptop').classList.remove('pin');return;}
 $('stoptop').classList.toggle('pin',!!f.pinned);
 $('scount').textContent=`${screen.index+1}/${screen.total}`;$('sident').textContent=val(f.ident,'UNKNOWN');$('sroute').textContent=route(f);$('saircraft').textContent=val(f.aircraft_display,f.aircraft_code||'');
 $('sairline').innerHTML=''; if(f.logo){const img=document.createElement('img');img.src='/api/logo?name='+encodeURIComponent(f.logo);$('sairline').appendChild(img);}else $('sairline').textContent=val(f.airline,f.operator_icao||'');
 const lines=statusLines(f);$('sline1').textContent=lines[0];$('sline2').textContent=lines[1];
 let progress=0;if(f.actual_out_epoch&&f.estimated_in_epoch>f.actual_out_epoch)progress=Math.min(100,Math.max(0,(Date.now()/1000-f.actual_out_epoch)/(f.estimated_in_epoch-f.actual_out_epoch)*100));$('sbar').style.width=progress+'%';
}
function fact(label,value){return `<div class="fact"><span>${esc(label)}</span><b>${esc(val(value))}</b></div>`}
function renderFlights(list){
 const nEnr=list.filter(f=>f.enriched).length;
 $('flightCount').textContent=list.length?`${list.length} card${list.length!==1?'s':''}${nEnr<list.length?' · '+nEnr+' enriched':''}`:' 0 flights';
 if(!list.length){$('flights').innerHTML='<div class="empty">No current flights.</div>';return;}
 $('flights').innerHTML=list.map(f=>{
  const baseTag=f.enriched?'<span class="tag">ENRICHED</span>':'<span class="tag tag-adsb">ADS-B</span>';
  const tag=`<div class="tags">${f.pinned?'<span class="tag tag-pin">PINNED</span>':''}${baseTag}</div>`;
  const hasPos=f.lat!=null&&f.lon!=null;
  const r=f.enriched?`<div class="route">${esc(route(f))}</div><div class="city">${esc(val(f.origin_city,''))}${f.destination_city?' → '+esc(f.destination_city):''}</div>`:`<div class="route" style="font-size:15px;color:var(--ink-dim)">${f.on_ground?'On ground':'Airborne'}</div><div class="city"></div>`;
  const pos=f.pinned?(hasPos?`<div class="city" style="margin-top:4px"><a href="https://www.google.com/maps?q=${f.lat},${f.lon}" target="_blank" rel="noopener" style="color:var(--amber);font-weight:600">&#128205; Show Location</a></div>`:`<div class="city" style="margin-top:4px;color:var(--amber)">&#128205; Locating&hellip;</div>`):'';
  return `<article class="flight${f.pinned?' pin':''}"><div class="flight-id"><h3>${esc(val(f.ident))}</h3>${tag}</div>${r}${pos}<div class="facts">${fact('Airline',f.airline||f.operator_icao)}${fact('Aircraft',f.aircraft_display||f.aircraft_code)}${fact('Distance',fixed(f.distance_km)+' km')}${fact('Bearing',fixed(f.bearing_deg,0)+' deg')}${fact('Latitude',hasPos?f.lat.toFixed(4):null)}${fact('Longitude',hasPos?f.lon.toFixed(4):null)}${fact('Altitude',f.baro_altitude_m!=null?Math.round(f.baro_altitude_m*3.28084)+' ft':null)}${fact('Geo alt',f.geo_altitude_m!=null?Math.round(f.geo_altitude_m*3.28084)+' ft':null)}${fact('Speed',f.velocity_mps!=null?Math.round(f.velocity_mps*3.6)+' km/h':null)}${fact('V/rate',f.vertical_rate_mps!=null?fixed(f.vertical_rate_mps)+' m/s':null)}${fact('Heading',f.heading_deg!=null?fixed(f.heading_deg,0)+' deg':null)}${fact('Departure',when(f.actual_out_epoch))}${fact('Arrival',when(f.estimated_in_epoch))}${fact('ICAO24',f.icao24)}${fact('Squawk',f.squawk)}${fact('Country',f.origin_country)}</div></article>`;
 }).join('');
}
function scrollCards(dir){$('flights').scrollBy({left:dir*290,behavior:'smooth'});}
function renderEvents(events){
 const el=$('events'), pinned=el.scrollHeight-el.scrollTop-el.clientHeight<35;
 el.innerHTML=events.length?events.slice().reverse().map(e=>`<div class="event">${esc(e)}</div>`).join(''):'<div class="empty">Waiting for a fetch cycle...</div>';
 if(pinned)el.scrollTop=0;
}
function updateBusyBanner(busy,phase){
 const el=$('busy-bar');
 if(!busy){el.classList.remove('show');return;}
 $('busy-text').textContent=phase?`Device busy: ${phase} — please do not refresh`:'Device busy — please do not refresh';
 el.classList.add('show');
}
async function poll(){
 const ctrl=new AbortController();
 const t=setTimeout(()=>ctrl.abort(),1500);
 try{const d=await (await fetch('/api/live',{cache:'no-store',signal:ctrl.signal})).json();clearTimeout(t);
 $('connection').textContent='Device live';$('clock').textContent=new Date().toLocaleTimeString();
 const nu=$('nextupd');if(d.next_fetch_in===undefined){nu.style.display='none';}else if(d.busy){nu.textContent='Fetch in progress';nu.style.display='';}else if(d.next_fetch_in<0){nu.textContent='First fetch pending';nu.style.display='';}else{nu.textContent='Next update '+d.next_fetch_in+'s';nu.style.display='';}
 renderScreen(d.screen);renderFlights(d.flights);renderEvents(d.events);
 updateBusyBanner(d.busy,d.phase||'');
 const al=$('api-alert');
 if(d.api_error){al.textContent='⚠ '+d.api_error;al.style.display='block';}
 else if(d.opensky_credits!==undefined&&d.opensky_credits<200){al.textContent='⚠ OpenSky credits critically low — '+d.opensky_credits+' remaining today. Raise fetch interval or wait for midnight reset.';al.style.display='block';}
 else{al.style.display='none';}
 if(d.opensky_credits!==undefined){const c=d.opensky_credits,el=$('credits');el.textContent=c+' API credits';el.style.color=c<200?'var(--red)':c<500?'var(--amber)':'var(--ink-dim)';el.style.display='';}
 g_mapFlights=d.flights||[];if($('mapwrap').style.display!=='none')updateMapOverlay();if($('tftmap').style.display!=='none'){var mt=$('tftmaptile');if(mt.complete&&mt.naturalWidth)updateMapOverlay(mt,$('tftmapol'));}
 }catch(e){clearTimeout(t);updateBusyBanner(true,e.name==='AbortError'?'Awaiting response…':'');$('connection').textContent='Device busy';}
}
async function loadConfig(){
 try{const d=await(await fetch('/api/config',{cache:'no-store'})).json();['lat','lon','brightness','fetch_sec','cycle_sec','map_sec'].forEach(k=>$(k).value=d[k]??'');$('radius').value=d.radius_km??'';if(d.label_color)$('labelColor').value=d.label_color;$('pinned_flight').value=d.pinned_flight??'';$('oskyIdState').textContent=d.opensky_configured?'configured':'';$('oskyState').textContent=d.opensky_configured?'configured':'';$('aeroState').textContent=d.aero_configured?'configured':'';configLoaded=true;updateMapOverlay();}catch(e){$('msg').textContent='Configuration unavailable';}
}
async function saveConfig(){
 const p={lat:parseFloat($('lat').value),lon:parseFloat($('lon').value),radius_km:Math.max(1,parseFloat($('radius').value)),fetch_sec:Math.max(10,parseInt($('fetch_sec').value)),cycle_sec:Math.max(1,parseInt($('cycle_sec').value)),map_sec:Math.max(5,parseInt($('map_sec').value)),brightness:Math.min(255,Math.max(0,parseInt($('brightness').value))),label_color:$('labelColor').value};
 if($('clear_osky').checked){p.osky_id='';p.osky_sec='';}else{if($('osky_id').value)p.osky_id=$('osky_id').value;if($('osky_sec').value)p.osky_sec=$('osky_sec').value;}
 if($('clear_aero').checked)p.aero_key=''; else if($('aero_key').value)p.aero_key=$('aero_key').value;
 $('save').disabled=true;try{const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(p)});$('msg').textContent=r.ok?'Settings applied — no reboot needed.':'Save failed.';}catch(e){$('msg').textContent='Save failed: '+e;}$('save').disabled=false;
}
async function rebootDevice(){if(!confirm('Reboot the device now?'))return;$('reboot').disabled=true;try{await fetch('/api/reboot',{method:'POST'});$('msg').textContent='Rebooting — reconnect in ~15 s.';}catch(e){$('msg').textContent='Reboot request failed: '+e;$('reboot').disabled=false;}}
async function savePinnedFlight(){const inp=$('pinned_flight'),m=$('pinnedMsg'),v=inp.value.trim().toUpperCase();try{const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pinned_flight:v})});m.style.color=r.ok?'var(--green)':'#e0533a';m.textContent=r.ok?(v?'✓ Saved '+v+' — fetching now…':'✓ Cleared.'):'Save failed.';if(r.ok){inp.style.outline='2px solid var(--green)';setTimeout(()=>inp.style.outline='',900);}setTimeout(()=>m.textContent='',5000);}catch(e){m.style.color='#e0533a';m.textContent='Error.';}}
function calcMapZoom(rKm,cLat,w,h){var cosLat=Math.max(0.01,Math.cos(cLat*Math.PI/180));var minDim=Math.min(w,Math.max(h-20,0));return Math.min(15,Math.max(8,Math.floor(Math.log2(minDim*40075*cosLat/(256*2.5*rKm)))));}
function llToXY(lat,lon,cLat,cLon,zoom,w,h){var scale=256*Math.pow(2,zoom);var dx=(lon-cLon)*scale/360;var mLat=Math.log(Math.tan(Math.PI/4+lat*Math.PI/360));var mCtr=Math.log(Math.tan(Math.PI/4+cLat*Math.PI/360));return {x:w/2+dx,y:h/2-(mLat-mCtr)*scale/(2*Math.PI)};}
function updateMapOverlay(imgEl,svgEl){
 var img=imgEl||$('maptile'),svg=svgEl||$('mapol');
 var w=img.naturalWidth,h=img.naturalHeight;
 if(!w||!h)return;
 var cx=w/2,cy=h/2,a='#ff9b2e';
 var rPx=Math.min(w,Math.max(h-20,0))/2.5;
 svg.setAttribute('viewBox','0 0 '+w+' '+h);
 var html=
  '<circle cx="'+cx+'" cy="'+cy+'" r="'+rPx+'" fill="none" stroke="'+a+'" stroke-width="1.5" stroke-dasharray="6 4" opacity="0.85"/>'+
  '<line x1="'+(cx-12)+'" y1="'+cy+'" x2="'+(cx+12)+'" y2="'+cy+'" stroke="'+a+'" stroke-width="1.5" opacity="0.9"/>'+
  '<line x1="'+cx+'" y1="'+(cy-12)+'" x2="'+cx+'" y2="'+(cy+12)+'" stroke="'+a+'" stroke-width="1.5" opacity="0.9"/>'+
  '<circle cx="'+cx+'" cy="'+cy+'" r="2.5" fill="'+a+'" opacity="0.95"/>';
 var sf=$('showFlights');
 if(sf&&sf.checked&&g_mapFlights.length){
  var cLat=parseFloat($('lat').value),cLon=parseFloat($('lon').value),rKm=parseFloat($('radius').value);
  if(!isNaN(cLat)&&!isNaN(cLon)&&!isNaN(rKm)){
   var zoom=calcMapZoom(rKm,cLat,w,h),placed=[],lblH=14;
   var userCol=$('labelColor')?$('labelColor').value:'#1e90ff';
   g_mapFlights.forEach(function(f){
    if(f.lat==null||f.lon==null)return;
    var pt=llToXY(f.lat,f.lon,cLat,cLon,zoom,w,h),fx=pt.x,fy=pt.y;
    if(fx<3||fx>w-3||fy<21||fy>h-3)return;
    var col=userCol;
    if(f.heading_deg!=null){var hr=f.heading_deg*Math.PI/180;html+='<line x1="'+fx.toFixed(1)+'" y1="'+fy.toFixed(1)+'" x2="'+(fx+Math.sin(hr)*10).toFixed(1)+'" y2="'+(fy-Math.cos(hr)*10).toFixed(1)+'" stroke="'+col+'" stroke-width="1.5"/>';}
    html+='<circle cx="'+fx.toFixed(1)+'" cy="'+fy.toFixed(1)+'" r="4" fill="#000" opacity="0.5"/><circle cx="'+fx.toFixed(1)+'" cy="'+fy.toFixed(1)+'" r="3" fill="'+col+'"/>';
    var lbl=(f.ident||f.icao24||'').substring(0,6);
    if(!lbl)return;
    var lblW=lbl.length*7+4;
    var cands=[[fx+6,fy-lblH-1],[fx-lblW-4,fy-lblH-1],[fx+6,fy+4],[fx-lblW-4,fy+4]];
    var lx=-1,ly=-1;
    for(var c=0;c<4;c++){var px=cands[c][0],py=cands[c][1];if(px<0||px+lblW>w||py<21||py+lblH>h)continue;if(!placed.some(function(b){return px<b.x+b.w&&px+lblW>b.x&&py<b.y+b.h&&py+lblH>b.y;})){lx=px;ly=py;break;}}
    if(lx>=0){html+='<text x="'+lx.toFixed(1)+'" y="'+(ly+lblH-2).toFixed(1)+'" font-family="monospace" font-size="12" fill="'+col+'" opacity="0.9">'+esc(lbl)+'</text>';placed.push({x:lx,y:ly,w:lblW,h:lblH});}
   });
  }
 }
 svg.innerHTML=html;
}
function refreshMapPreview(){var img=$('maptile');img.onerror=function(){$('mapwrap').style.display='none';};img.onload=function(){$('mapwrap').style.display='';updateMapOverlay();};img.src='/api/mappreview?t='+Date.now();var mt=$('tftmaptile');if(mt)mt.dataset.stale='1';}
async function fetchMap(btn){
 const lat=parseFloat($('lat').value),lon=parseFloat($('lon').value),r=parseFloat($('radius').value);
 if(isNaN(lat)||isNaN(lon)||isNaN(r)){$('msg').textContent='Enter valid lat, lon and radius first.';return;}
 btn.disabled=true;btn.textContent='Fetching…';$('msg').textContent='Map fetch sent — CYD will update in ~10s.';
 try{const res=await fetch('/api/fetchmap',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({lat,lon,radius_km:r})});
 btn.textContent=res.ok?'Done!':'Failed';if(res.ok)setTimeout(refreshMapPreview,10000);}
 catch(e){btn.textContent='Error';}
 setTimeout(()=>{btn.disabled=false;btn.textContent='Fetch Map';},5000);}
loadConfig();poll();setInterval(poll,1000);refreshMapPreview();
</script></body></html>)rawlit";

static String timestampNow()
{
  const time_t now = time(nullptr);
  if (now <= 1000000000L)
    return String("boot +") + String(millis() / 1000UL) + "s";
  char out[20];
  struct tm local = {};
  localtime_r(&now, &local);
  strftime(out, sizeof(out), "%H:%M:%S", &local);
  return String(out);
}

static String logoName(const String &path)
{
  const int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

static void addOptionalDouble(JsonObject object, const char *key, double value)
{
  if (!isnan(value))
    object[key] = value;
}

static void addFlight(JsonObject object, const FlightInfo &flight)
{
  object["enriched"] = flight.enriched;
  object["pinned"]   = flight.pinned;
  // ident: preferred display callsign — mirrors CYDDisplay::resolveCallsign() so the
  // WebUI shows the same form as the TFT (e.g. VA804 — IATA — rather than VOZ804 — ICAO).
  // ident_icao and ident_iata remain available below for callers that need the raw values.
  object["ident"] = flight.ident_iata.length() ? flight.ident_iata
                  : flight.ident.length()      ? flight.ident
                  :                              flight.ident_icao;
  object["ident_icao"] = flight.ident_icao;
  object["ident_iata"] = flight.ident_iata;
  object["operator"] = flight.operator_code;
  object["operator_icao"] = flight.operator_icao;
  object["operator_iata"] = flight.operator_iata;
  object["airline"] = flight.airline_display_name_full;
  object["aircraft_code"] = flight.aircraft_code;
  object["aircraft_display"] = flight.aircraft_display_name_short;
  object["icao24"] = flight.icao24;
  object["origin_country"] = flight.origin_country;
  object["origin_icao"] = flight.origin.code_icao;
  object["origin_iata"] = flight.origin.code_iata;
  object["origin_city"] = flight.origin.city;
  object["destination_icao"] = flight.destination.code_icao;
  object["destination_iata"] = flight.destination.code_iata;
  object["destination_city"] = flight.destination.city;
  object["actual_out_epoch"] = (unsigned long)flight.actual_out_epoch;
  object["estimated_in_epoch"] = (unsigned long)flight.estimated_in_epoch;
  object["time_position"] = flight.time_position;
  object["last_contact"] = flight.last_contact;
  object["squawk"] = flight.squawk;
  object["position_source"] = flight.position_source;
  object["on_ground"] = flight.on_ground;
  addOptionalDouble(object, "lat", flight.lat);
  addOptionalDouble(object, "lon", flight.lon);
  addOptionalDouble(object, "distance_km", flight.distance_km);
  addOptionalDouble(object, "bearing_deg", flight.bearing_deg);
  addOptionalDouble(object, "baro_altitude_m", flight.baro_altitude_m);
  addOptionalDouble(object, "geo_altitude_m", flight.geo_altitude_m);
  addOptionalDouble(object, "velocity_mps", flight.velocity_mps);
  addOptionalDouble(object, "heading_deg", flight.heading_deg);
  addOptionalDouble(object, "vertical_rate_mps", flight.vertical_rate_mps);
  if (flight.logo_path.length())
    object["logo"] = logoName(flight.logo_path);
}

void WebUIServer::begin(const std::vector<FlightInfo> *flights, CYDDisplay *display)
{
  _flights = flights;
  _display = display;
  _server.on("/", HTTP_GET, [this] { onRoot(); });
  _server.on("/api/config", HTTP_GET, [this] { onGetConfig(); });
  _server.on("/api/config", HTTP_POST, [this] { onPostConfig(); });
  _server.on("/api/reboot", HTTP_POST, [this] { onReboot(); });
  _server.on("/api/live", HTTP_GET, [this] { onGetLive(); });
  _server.on("/api/logo", HTTP_GET, [this] { onGetLogo(); });
  _server.on("/api/fetchmap",   HTTP_POST, [this] { onFetchMap(); });
  _server.on("/api/mappreview", HTTP_GET,  [this] { onGetMapPreview(); });
  _server.on("/api/screenshot", HTTP_GET,  [this] { onGetScreenshot(); });
  _server.onNotFound([this] { onNotFound(); });
  _server.begin();
  appendEvent("Web dashboard ready; waiting for flight data.");
  DBG_INFO("WebUI: HTTP dashboard listening on port 80");
}

void WebUIServer::handle()
{
  _server.handleClient();
}

void WebUIServer::appendEvent(const String &message)
{
  const String line = timestampNow() + "  " + message;
  if (_eventCount < EVENT_CAPACITY)
  {
    _events[(_eventStart + _eventCount) % EVENT_CAPACITY] = line;
    _eventCount++;
    return;
  }
  _events[_eventStart] = line;
  _eventStart = (_eventStart + 1) % EVENT_CAPACITY;
}

static String cardinalFromBearing(double deg)
{
  if (isnan(deg)) return String();
  static const char *dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  const int idx = ((int)((deg + 22.5) / 45.0)) & 7;
  return String(dirs[idx]);
}

static String describeAltitude(double baroM)
{
  if (isnan(baroM) || baroM <= 0) return String();
  const long ft = lround(baroM * 3.28084);
  if (ft >= 18000) return String("FL") + String((int)lround(ft / 100.0));
  return String((int)lround(ft / 100.0) * 100) + "ft";
}

// Build a human-readable single-line event describing one observed aircraft.
// Merges the raw OpenSky StateVector with an optional matching enriched
// FlightInfo (matched by icao24). Designed to make sense to a non-technical
// reader: callsign first, then route or current status, then physical metrics.
static String describeAircraft(const StateVector &s, const FlightInfo *match)
{
  String ident = s.callsign;
  ident.trim();
  const bool hasCallsign = ident.length() > 0;
  if (!hasCallsign) ident = s.icao24;

  String line = ident;

  // Route / status descriptor
  if (match && match->enriched)
  {
    const String fromCity = match->origin.city.length()      ? match->origin.city
                          : match->origin.code_iata.length() ? match->origin.code_iata
                                                             : match->origin.code_icao;
    const String toCity   = match->destination.city.length()      ? match->destination.city
                          : match->destination.code_iata.length() ? match->destination.code_iata
                                                                  : match->destination.code_icao;
    if (fromCity.length() && toCity.length())
      line += " — " + fromCity + " → " + toCity;
    else if (toCity.length())
      line += " — to " + toCity;

    String tag;
    if (match->airline_display_name_full.length()) tag = match->airline_display_name_full;
    if (match->aircraft_display_name_short.length())
    {
      if (tag.length()) tag += " ";
      tag += match->aircraft_display_name_short;
    }
    if (tag.length()) line += " (" + tag + ")";
  }
  else if (!hasCallsign)
  {
    line += " — unidentified ADS-B contact";
    if (s.origin_country.length()) line += " (" + s.origin_country + ")";
  }
  else if (s.on_ground)
  {
    line += " — on ground, no active flight match";
  }
  else
  {
    line += " — airborne, no route data";
  }

  // Physical metrics
  String metrics;
  if (!isnan(s.distance_km))
  {
    metrics = String((int)lround(s.distance_km)) + "km";
    const String dir = cardinalFromBearing(s.bearing_deg);
    if (dir.length()) metrics += " " + dir;
  }
  if (s.on_ground)
  {
    if (metrics.length()) metrics += " · ";
    metrics += "on ground";
  }
  else
  {
    const String alt = describeAltitude(s.baro_altitude);
    if (alt.length())
    {
      if (metrics.length()) metrics += " · ";
      metrics += alt;
    }
    if (!isnan(s.velocity))
    {
      const long kmh = lround(s.velocity * 3.6);
      if (kmh > 0)
      {
        if (metrics.length()) metrics += " · ";
        metrics += String(kmh) + "km/h";
      }
    }
    if (!isnan(s.vertical_rate) && fabs(s.vertical_rate) >= 1.0)
      metrics += s.vertical_rate > 0 ? " climbing" : " descending";
  }
  if (metrics.length()) line += "  " + metrics;

  return line;
}

void WebUIServer::recordFetch(const std::vector<StateVector> &states,
                              const std::vector<FlightInfo> &flights,
                              size_t enriched)
{
  _lastFetchEpoch = time(nullptr);
  _lastFetchMs    = millis();

  // Summary in plain language
  String summary = String("Observed ") + String(states.size()) + " nearby aircraft";
  if (enriched > 0)
    summary += "; " + String(enriched) + " matched to a current flight";
  summary += "; " + String(flights.size()) + " shown on display.";
  appendEvent(summary);

  // Per-aircraft detail. Iterate over raw OpenSky observations so even cards
  // removed by the displayability filter (parked, hex-only ident) are listed.
  // 12 keeps roughly 4 fetches of history inside the 50-entry ring buffer.
  static constexpr size_t MAX_DETAIL_LINES = 12;
  size_t shown = 0;
  for (const StateVector &s : states)
  {
    if (shown >= MAX_DETAIL_LINES)
    {
      appendEvent(String("… and ") + String(states.size() - shown) + " more not listed");
      break;
    }
    // Find an enriched/displayable match by icao24
    const FlightInfo *match = nullptr;
    for (const FlightInfo &f : flights)
    {
      if (f.icao24 == s.icao24) { match = &f; break; }
    }
    appendEvent(describeAircraft(s, match));
    shown++;
  }
}

void WebUIServer::setCreditsRemaining(int n)
{
  const bool wasAboveThreshold = _creditsRemaining < 0 || _creditsRemaining >= 300;
  _creditsRemaining = n;
  if (n >= 0 && n < 300 && wasAboveThreshold)
  {
    appendEvent(String("Warning: OpenSky credits low — ") + String(n) + " remaining today");
    _creditsWarned = true;
  }
  else if (n >= 500)
  {
    _creditsWarned = false;
  }
}

void WebUIServer::onRoot()
{
  _server.sendHeader("Cache-Control", "no-cache, no-store");
  _server.send_P(200, "text/html", HTML_PAGE);
}

void WebUIServer::onFetchMap()
{
  const String body = _server.arg("plain");
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, body) || !doc.containsKey("lat") || !doc.containsKey("lon"))
  {
    _server.send(400, "application/json", "{\"error\":\"lat and lon required\"}");
    return;
  }

  // Update RuntimeConfig in memory only — no NVS save, no reboot.
  // ensureMapCached() will detect the change (coords differ from cached metadata)
  // and re-fetch the map tile for these coordinates.
  RuntimeConfig::setCenterLat(doc["lat"].as<double>());
  RuntimeConfig::setCenterLon(doc["lon"].as<double>());
  if (doc.containsKey("radius_km"))
    RuntimeConfig::setRadiusKm(doc["radius_km"].as<double>());

  DBG_INFO("WebUI: fetch map for %.5f,%.5f r=%.1fkm",
           RuntimeConfig::centerLat(), RuntimeConfig::centerLon(), RuntimeConfig::radiusKm());
  _server.sendHeader("Cache-Control", "no-cache, no-store");
  _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUIServer::onGetMapPreview()
{
  File f = LittleFS.open("/mapcache.jpg", "r");
  if (!f)
  {
    _server.send(404, "text/plain", "No map cached — use Fetch Map first");
    return;
  }
  _server.sendHeader("Cache-Control", "no-cache, no-store");
  _server.streamFile(f, "image/jpeg");
  f.close();
}

// ── /api/screenshot ──────────────────────────────────────────────────────────
// Streams a 24-bit BMP of the live CYD framebuffer. Pixel readback uses
// TFT_eSPI::readRectRGB (RGB888, no byte-swap); rows are converted to BMP's
// BGR order in place and streamed via WebServer::sendContent.
//
// File sizes: 320×240 → 230,454 B  ·  480×320 → 460,854 B
// Stack use:  1494 B (1440 row + 54 header)  — zero heap allocations.
//
// SPI safety: handleClient() runs synchronously in loop() so no concurrent
// TFT writes can occur during the readback. Display freezes visually for
// ~150–200 ms during capture; total transfer ~1–2 s on typical WiFi.
//
// Known risk: some ILI9341 clones do not implement the RAMRD (0x2E) command
// — the resulting BMP will be black/garbled. Diagnostic: drop the
// -DSPI_READ_FREQUENCY build flag from 20 MHz to 6.25 MHz and retry.
void WebUIServer::onGetScreenshot()
{
  if (!_display)
  {
    _server.send(503, "text/plain", "Display not ready");
    return;
  }

  const int16_t  w        = _display->width();
  const int16_t  h        = _display->height();
  const uint32_t rowBytes = (uint32_t)w * 3;
  const uint32_t pixBytes = rowBytes * h;
  const uint32_t fileSize = 54 + pixBytes;

  // Row buffer sized for max-supported display width (480 px). Both
  // 320×3=960 and 480×3=1440 are 4-byte aligned, so no BMP padding needed.
  static constexpr int16_t MAX_W = 480;
  if (w > MAX_W)
  {
    _server.send(500, "text/plain", "Display width exceeds screenshot buffer");
    return;
  }
  uint8_t row[MAX_W * 3];   // 1440 B on stack

  // ── BMP header (54 B) ──
  // BITMAPFILEHEADER (14 B) + BITMAPINFOHEADER (40 B), all little-endian.
  // biHeight is negative so pixel rows are stored top-to-bottom, matching
  // the natural screen scan order.
  uint8_t hdr[54] = {0};
  auto le16 = [](uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
  };
  auto le32 = [](uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
  };
  hdr[0] = 'B'; hdr[1] = 'M';
  le32(hdr +  2, fileSize);          // file size
  le32(hdr + 10, 54);                // pixel data offset
  le32(hdr + 14, 40);                // DIB header size
  le32(hdr + 18, (uint32_t)w);       // width
  le32(hdr + 22, (uint32_t)(-h));    // height (negative → top-to-bottom)
  le16(hdr + 26, 1);                 // colour planes
  le16(hdr + 28, 24);                // bits per pixel
  // bytes 30..53 already zero: BI_RGB compression, image size=0, ppm=0, palette=0

  // ── Filename: timestamped if NTP synced, millis fallback before sync ──
  char fname[40];
  const time_t now = time(nullptr);
  if (now > 1000000000L)
  {
    struct tm lt;
    localtime_r(&now, &lt);
    snprintf(fname, sizeof(fname),
             "cyd_%04d%02d%02d_%02d%02d%02d.bmp",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
             lt.tm_hour, lt.tm_min, lt.tm_sec);
  }
  else
  {
    snprintf(fname, sizeof(fname), "cyd_t%lu.bmp", (unsigned long)millis());
  }
  char cd[80];
  snprintf(cd, sizeof(cd), "attachment; filename=\"%s\"", fname);

  // ── Stream ──
  _server.sendHeader("Content-Disposition", cd);
  _server.sendHeader("Cache-Control", "no-store");
  _server.setContentLength(fileSize);
  _server.send(200, "image/bmp", "");
  _server.sendContent((const char *)hdr, 54);

  const unsigned long t0 = millis();
  for (int16_t y = 0; y < h; y++)
  {
    _display->readRectRGB(0, y, w, 1, row);
    // readRectRGB returns R,G,B per pixel — BMP needs B,G,R. Swap in place.
    for (int16_t i = 0; i < w; i++)
    {
      uint8_t tmp     = row[i * 3 + 0];
      row[i * 3 + 0]  = row[i * 3 + 2];
      row[i * 3 + 2]  = tmp;
    }
    _server.sendContent((const char *)row, rowBytes);
  }
  DBG_INFO("Screenshot: %s served (%u bytes, %lu ms)",
           fname, (unsigned)fileSize, millis() - t0);
}

void WebUIServer::onGetConfig()
{
  DynamicJsonDocument doc(768);
  doc["lat"] = RuntimeConfig::centerLat();
  doc["lon"] = RuntimeConfig::centerLon();
  doc["radius_km"] = RuntimeConfig::radiusKm();
  doc["fetch_sec"] = RuntimeConfig::fetchIntervalSec();
  doc["cycle_sec"] = RuntimeConfig::displayCycleSec();
  doc["map_sec"]   = RuntimeConfig::mapDisplaySec();
  doc["brightness"] = RuntimeConfig::brightness();
  {
    const uint16_t c = RuntimeConfig::labelColor();
    uint8_t r = (c >> 11) & 0x1F; r = (r << 3) | (r >> 2);
    uint8_t g = (c >>  5) & 0x3F; g = (g << 2) | (g >> 4);
    uint8_t b =  c        & 0x1F; b = (b << 3) | (b >> 2);
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    doc["label_color"] = buf;
  }
  doc["opensky_configured"] =
      RuntimeConfig::openskyClientId().length() && RuntimeConfig::openskyClientSecret().length();
  doc["aero_configured"]   = RuntimeConfig::aeroApiKey().length() > 0;
  doc["pinned_flight"]     = RuntimeConfig::pinnedFlightNumber();

  String out;
  serializeJson(doc, out);
  _server.sendHeader("Cache-Control", "no-cache, no-store");
  _server.send(200, "application/json", out);
}

void WebUIServer::onPostConfig()
{
  const String body = _server.arg("plain");
  if (body.isEmpty())
  {
    _server.send(400, "application/json", "{\"error\":\"empty body\"}");
    return;
  }

  DynamicJsonDocument doc(1024);
  const DeserializationError err = deserializeJson(doc, body);
  if (err)
  {
    DBG_WARN("WebUI: POST JSON parse error: %s", err.c_str());
    _server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }

  if (doc.containsKey("lat")) RuntimeConfig::setCenterLat(doc["lat"].as<double>());
  if (doc.containsKey("lon")) RuntimeConfig::setCenterLon(doc["lon"].as<double>());
  if (doc.containsKey("radius_km")) RuntimeConfig::setRadiusKm(doc["radius_km"].as<double>());
  if (doc.containsKey("fetch_sec")) RuntimeConfig::setFetchIntervalSec(doc["fetch_sec"].as<uint32_t>());
  if (doc.containsKey("cycle_sec")) RuntimeConfig::setDisplayCycleSec(doc["cycle_sec"].as<uint32_t>());
  if (doc.containsKey("map_sec"))   RuntimeConfig::setMapDisplaySec(doc["map_sec"].as<uint32_t>());
  if (doc.containsKey("brightness")) RuntimeConfig::setBrightness(doc["brightness"].as<uint8_t>());
  if (doc.containsKey("label_color"))
  {
    const char *hex = doc["label_color"].as<const char *>();
    if (hex && hex[0] == '#' && strlen(hex) >= 7)
    {
      unsigned int r = 0, g = 0, b = 0;
      if (sscanf(hex + 1, "%2x%2x%2x", &r, &g, &b) == 3)
      {
        const uint16_t rgb565 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        RuntimeConfig::setLabelColor(rgb565);
      }
    }
  }
  if (doc.containsKey("osky_id")) RuntimeConfig::setOpenskyClientId(doc["osky_id"].as<String>());
  if (doc.containsKey("osky_sec")) RuntimeConfig::setOpenskyClientSecret(doc["osky_sec"].as<String>());
  if (doc.containsKey("aero_key")) RuntimeConfig::setAeroApiKey(doc["aero_key"].as<String>());
  if (doc.containsKey("pinned_flight"))
  {
    String pf = doc["pinned_flight"].as<String>();
    pf.trim();
    pf.toUpperCase();
    RuntimeConfig::setPinnedFlightNumber(pf);
    // Signal the main loop to act immediately — set a new pin (fetch + placeholder)
    // or clear an existing one (strip the stale card without delay).
    _pendingForceFetch = true;
  }

  RuntimeConfig::save();
  _server.send(200, "application/json", "{\"ok\":true}");

  // All settings apply live — no reboot. The main loop reads RuntimeConfig fresh
  // each cycle; these flags tell it to apply changes that need an explicit action
  // (backlight write, OAuth token refresh) and to refresh promptly rather than
  // waiting for the next fetch interval.
  if (doc.containsKey("osky_id") || doc.containsKey("osky_sec"))
    _pendingReauth = true; // creds changed — drop cached OpenSky token

  // Any non-pinned change applies live (brightness, label colour, location, timing).
  const bool onlyPinnedFlight = doc.size() == 1 && doc.containsKey("pinned_flight");
  if (!onlyPinnedFlight)
    _pendingApply = true;

  DBG_INFO("WebUI: config saved (live apply, no reboot)");
}

void WebUIServer::onReboot()
{
  _server.send(200, "application/json", "{\"ok\":true}");
  DBG_INFO("WebUI: reboot requested by user");
  _pendingReboot = true; // main loop reboots after a short flush delay
}

void WebUIServer::onGetLive()
{
  DynamicJsonDocument doc(32768);
  JsonObject screen = doc.createNestedObject("screen");
  screen["width"] = _display ? _display->width() : 0;
  screen["height"] = _display ? _display->height() : 0;
  // CYDDisplay::displayFlights() cycles through flights.size() + 1 slots — the +1
  // is the radar map card. Mirror that exact logic here so the WebUI knows which
  // slot is currently on the TFT (flight N or the map). Without the +1 the modulo
  // collapses the map slot back to flight #0 and the WebUI never sees the map.
  // The map slot is only present when at least one flight has a plottable
  // position — mirror CYDDisplay::displayFlights() exactly (anyFlightLocatable).
  const size_t flightCount = _flights ? _flights->size() : 0;
  const bool   hasMap      = _flights && anyFlightLocatable(*_flights);
  const size_t totalSlots  = flightCount + (hasMap ? 1 : 0);
  const size_t rawIdx      = _display ? _display->currentFlightIndex() : 0;
  const size_t slotIdx     = totalSlots ? rawIdx % totalSlots : 0;
  const bool   isMap       = hasMap && (slotIdx == flightCount);
  screen["total"] = flightCount;
  screen["index"] = isMap ? flightCount : slotIdx;
  screen["kind"]  = isMap ? "map" : "flight";
  if (!isMap && flightCount)
    addFlight(screen.createNestedObject("flight"), (*_flights)[slotIdx]);

  JsonArray flightsArr = doc.createNestedArray("flights");
  if (_flights)
  {
    for (const FlightInfo &flight : *_flights)
      addFlight(flightsArr.createNestedObject(), flight);
  }

  JsonArray events = doc.createNestedArray("events");
  for (size_t i = 0; i < _eventCount; i++)
    events.add(_events[(_eventStart + i) % EVENT_CAPACITY]);
  doc["last_fetch"] = (unsigned long)_lastFetchEpoch;
  // next_fetch_in: seconds until the next scheduled fetch. -1 = no fetch yet (startup grace);
  // 0 = due now or overdue. Driven by millis() not wall-clock to avoid browser/device skew.
  {
    int32_t nextIn = -1;
    if (_lastFetchEpoch != 0)
    {
      const unsigned long intervalMs = RuntimeConfig::fetchIntervalSec() * 1000UL;
      const unsigned long elapsed    = millis() - _lastFetchMs;
      nextIn = (elapsed >= intervalMs) ? 0
                                       : (int32_t)((intervalMs - elapsed + 999UL) / 1000UL);
    }
    doc["next_fetch_in"] = nextIn;
  }
  if (_creditsRemaining >= 0) doc["opensky_credits"] = _creditsRemaining;
  if (_apiError.length()) doc["api_error"] = _apiError;
  doc["busy"]  = _busy;
  doc["phase"] = _phase;

  String out;
  serializeJson(doc, out);
  _server.sendHeader("Cache-Control", "no-cache, no-store");
  _server.send(200, "application/json", out);
}

void WebUIServer::onGetLogo()
{
  const String name = _server.arg("name");
  if (name.indexOf('/') >= 0 || name.indexOf("..") >= 0 || !name.endsWith(".jpg"))
  {
    _server.send(400, "text/plain", "Invalid logo");
    return;
  }
  const String path = String("/logos/") + name;
  File file = LittleFS.open(path, "r");
  if (!file)
  {
    _server.send(404, "text/plain", "Logo not found");
    return;
  }
  _server.sendHeader("Cache-Control", "public, max-age=86400");
  _server.streamFile(file, "image/jpeg");
  file.close();
}

void WebUIServer::onNotFound()
{
  _server.send(404, "text/plain", "Not found");
}
