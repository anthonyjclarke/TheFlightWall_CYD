#include "WebUIServer.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <math.h>
#include <time.h>
#include "CYDDisplay.h"
#include "RuntimeConfig.h"
#include "debug.h"

// Single-file dashboard application. Data is polled from the device so the
// ESP32 does not need WebSocket buffering or any persistent log storage.
static const char HTML_PAGE[] PROGMEM = R"rawlit(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>The Flight Wall &#8212; CYD v1.0.0</title>
<style>
:root{--bg:#060a0e;--panel:#0c1218;--panel2:#111a21;--line:#1d2b34;--muted:#71808b;--text:#e8edf0;--amber:#f6a23a;--green:#48d095;--blue:#58a8c9;--red:#e87063;--glow:rgba(246,162,58,.17)}
*{box-sizing:border-box}html,body{margin:0;min-height:100%;background:var(--bg);color:var(--text);font-family:Inter,"Avenir Next",system-ui,sans-serif}
body{background-image:radial-gradient(circle at 18% 0,rgba(246,162,58,.08),transparent 32rem),linear-gradient(135deg,#060a0e,#080f13 62%,#060a0e)}
.shell{max-width:1380px;margin:auto;padding:24px}
header{display:flex;align-items:flex-end;justify-content:space-between;gap:18px;margin-bottom:22px}
.brand small,.eyebrow{display:block;color:var(--amber);font-size:11px;letter-spacing:.22em;text-transform:uppercase;margin-bottom:9px}
h1{font-size:clamp(26px,3vw,38px);font-weight:540;line-height:1;margin:0;letter-spacing:-.04em}
.status{display:flex;align-items:center;gap:12px;border:1px solid var(--line);background:rgba(12,18,24,.75);border-radius:30px;padding:9px 15px;font-size:13px;color:var(--muted)}
.dot{width:9px;height:9px;border-radius:50%;background:var(--green);box-shadow:0 0 12px var(--green)}
.grid{display:grid;grid-template-columns:minmax(350px,520px) minmax(350px,1fr);gap:18px;margin-bottom:18px}
.panel{background:linear-gradient(145deg,rgba(15,23,29,.94),rgba(8,13,18,.96));border:1px solid var(--line);border-radius:18px;box-shadow:0 16px 44px rgba(0,0,0,.22);overflow:hidden}
.panel-head{display:flex;justify-content:space-between;align-items:center;padding:18px 20px 12px}
.panel-title{font-size:12px;color:var(--muted);letter-spacing:.16em;text-transform:uppercase}
.pill{border:1px solid #223641;padding:4px 10px;border-radius:20px;color:var(--blue);font-size:11px}
.monitor-wrap{padding:10px 20px 23px}
.bezel{background:#121a20;border-radius:16px;padding:14px;box-shadow:inset 0 0 0 1px #29343c,0 22px 36px rgba(0,0,0,.35)}
.tft{width:100%;aspect-ratio:4/3;background:#000;border-radius:4px;display:flex;flex-direction:column;color:#fff;font-family:Arial,sans-serif;overflow:hidden}
.tft[data-wide=true]{aspect-ratio:3/2}
.tft-top{height:17%;display:flex;align-items:center;border-bottom:1px solid #272d30;padding:0 3%;gap:18px}
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
.monitor-foot{display:flex;justify-content:space-between;color:var(--muted);font-size:12px;padding-top:14px}
.stream{height:390px;overflow:auto;padding:4px 20px 20px;font-family:"SFMono-Regular",ui-monospace,monospace;font-size:12px}
.event{padding:10px 0;border-bottom:1px solid rgba(29,43,52,.72);line-height:1.55;color:#c4ced4}
.event:first-child{color:#e7ecee}.empty{color:var(--muted);padding:22px 0}
.section{margin-bottom:18px}.cards{display:grid;grid-auto-flow:column;grid-auto-columns:minmax(230px,280px);gap:12px;padding:0 18px 18px;overflow-x:auto;scroll-behavior:smooth;-webkit-overflow-scrolling:touch}.cards::-webkit-scrollbar{height:4px}.cards::-webkit-scrollbar-thumb{background:var(--line);border-radius:2px}.nav-btn{background:transparent;border:1px solid var(--line);color:var(--muted);width:26px;height:26px;border-radius:6px;font-size:18px;line-height:1;cursor:pointer;padding:0;display:inline-flex;align-items:center;justify-content:center}.nav-btn:hover{border-color:var(--amber);color:var(--amber);background:rgba(246,162,58,.1)}.tag-adsb{color:var(--blue);border-color:rgba(88,168,201,.32)}
.flight{background:var(--panel2);border:1px solid var(--line);border-radius:14px;padding:15px;min-height:250px}
.flight-id{display:flex;justify-content:space-between;align-items:start;margin-bottom:15px}.flight h3{font-size:22px;margin:0;letter-spacing:-.02em}
.tag{color:var(--green);border:1px solid rgba(72,208,149,.32);font-size:10px;border-radius:12px;padding:3px 7px;letter-spacing:.1em}
.route{font-size:22px;color:var(--amber);font-weight:650;margin-bottom:4px}.city{font-size:12px;color:var(--muted);min-height:32px;margin-bottom:12px}
.facts{display:grid;grid-template-columns:1fr 1fr;gap:10px 8px}.fact span{display:block;color:var(--muted);font-size:10px;text-transform:uppercase;letter-spacing:.11em;margin-bottom:3px}.fact b{font-size:13px;font-weight:500}
.settings{padding:0 20px 22px}.settings-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}.field label{display:block;color:var(--muted);font-size:11px;letter-spacing:.1em;text-transform:uppercase;margin:0 0 6px}
input{width:100%;background:#0a1015;border:1px solid var(--line);border-radius:8px;padding:9px 10px;color:var(--text);font-size:13px}input:focus{outline:none;border-color:var(--amber)}
.credentials{margin-top:16px;display:grid;grid-template-columns:repeat(3,1fr);gap:12px}.cred-note{font-size:11px;color:var(--muted);margin-top:6px}.configured{color:var(--green)}
.actions{display:flex;justify-content:space-between;align-items:center;margin-top:18px;gap:10px}.message{font-size:13px;color:var(--muted)}
button{background:var(--amber);border:0;border-radius:9px;padding:11px 18px;font-weight:650;cursor:pointer;color:#181005}button:disabled{opacity:.45;cursor:default}
@media(max-width:1120px){.settings-grid,.credentials{grid-template-columns:repeat(2,1fr)}}
@media(max-width:800px){.shell{padding:14px}.grid{grid-template-columns:1fr}.stream{height:280px}.settings-grid,.credentials{grid-template-columns:1fr}header{align-items:start;flex-direction:column}}
footer{margin-top:28px;padding:22px 0 8px;border-top:1px solid var(--line);display:grid;grid-template-columns:1fr 2fr;gap:24px;color:var(--muted);font-size:12px}
footer h4{color:var(--text);font-size:11px;letter-spacing:.15em;text-transform:uppercase;margin:0 0 8px}footer a{color:var(--blue);text-decoration:none}footer a:hover{text-decoration:underline}
.ack{font-size:11px;color:var(--muted);text-align:center;padding:12px 0 4px;border-top:1px solid var(--line);margin-top:16px}
@media(max-width:800px){footer{grid-template-columns:1fr}}
</style>
</head>
<body><div class="shell">
<header><div class="brand"><small>The Flight Wall &middot; CYD Edition</small><h1>The Flight Wall <span style="font-size:18px;opacity:.45;font-weight:400;letter-spacing:.02em">v1.0.0</span></h1></div><div class="status"><span class="dot"></span><span id="connection">Connecting to device</span><span id="clock"></span></div></header>
<div class="grid">
 <section class="panel"><div class="panel-head"><span class="panel-title">TFT Mirror</span><span class="pill">Browser Replica / Low Impact</span></div><div class="monitor-wrap"><div class="bezel"><div class="tft" id="tft"><div class="tft-top"><span class="tft-count" id="scount">0/0</span><span class="tft-ident" id="sident">SEARCHING...</span></div><div class="tft-mid"><div class="tft-airline" id="sairline"></div><div><div class="tft-route" id="sroute">--- - ---</div><div class="tft-aircraft" id="saircraft"></div></div></div><div class="tft-status"><div id="sline1"></div><div id="sline2"></div></div><div class="bar"><span id="sbar"></span></div></div></div><div class="monitor-foot"><span id="resolution">Display unavailable</span><span id="cardtime"></span></div></div></section>
 <section class="panel"><div class="panel-head"><span class="panel-title">Flight Data Feed</span><span class="pill">Volatile / API Reads</span></div><div class="stream" id="events"><div class="empty">Waiting for a fetch cycle...</div></div></section>
</div>
<section class="panel section"><div class="panel-head"><span class="panel-title">Current Flights</span><div style="display:flex;align-items:center;gap:8px"><button class="nav-btn" onclick="scrollCards(-1)">&#8249;</button><span class="pill" id="flightCount">0 flights</span><button class="nav-btn" onclick="scrollCards(1)">&#8250;</button></div></div><div class="cards" id="flights"><div class="empty">No current flights.</div></div></section>
<section class="panel"><div class="panel-head"><span class="panel-title">Device Configuration</span><span class="pill">Protected Credentials</span></div><div class="settings">
 <div class="settings-grid">
  <div class="field"><label>Latitude</label><input type="number" id="lat" step="0.0001"></div><div class="field"><label>Longitude</label><input type="number" id="lon" step="0.0001"></div>
  <div class="field"><label>Radius km</label><input type="number" id="radius" min="1" max="500"></div><div class="field"><label>Brightness</label><input type="number" id="brightness" min="0" max="255"></div>
  <div class="field"><label>Fetch interval sec</label><input type="number" id="fetch_sec" min="10" max="3600"></div><div class="field"><label>Card cycle sec</label><input type="number" id="cycle_sec" min="1" max="60"></div>
 </div>
 <div class="credentials">
  <div class="field"><label>OpenSky Client ID <span id="oskyIdState"></span></label><input type="password" id="osky_id" placeholder="Leave blank to retain"><div class="cred-note"><input type="checkbox" id="clear_osky" style="width:auto"> Clear OpenSky credentials</div></div>
  <div class="field"><label>OpenSky Secret <span id="oskyState"></span></label><input type="password" id="osky_sec" placeholder="Leave blank to retain"></div>
  <div class="field"><label>AeroAPI Key <span id="aeroState"></span></label><input type="password" id="aero_key" placeholder="Leave blank to retain"><div class="cred-note"><input type="checkbox" id="clear_aero" style="width:auto"> Clear AeroAPI key</div></div>
 </div>
 <div class="actions"><span class="message" id="msg">Credential values are never returned to this page.</span><button id="save" onclick="saveConfig()">Save &amp; Reboot</button></div>
</div></section>
<footer>
<div><h4>Contact</h4><div>Anthony Clarke</div><div><a href="mailto:anthonyjclarke [at] gmail.com">anthonyjclarke [@] gmail.com</a></div><div style="margin-top:8px"><a href="https://github.com/anthonyjclarke">GitHub</a> &middot; <a href="https://bsky.app/profile/anthonyjclarke.bsky.social">BlueSky</a> &middot; <a href="https://www.threads.net/@anthonyjclarke">Threads</a> &middot; <a href="https://www.linkedin.com/in/anthonyjclarke">LinkedIn</a></div></div>
<div><h4>Origin</h4><div style="margin-bottom:12px;font-size:13px"><a href="https://github.com/AxisNimble/TheFlightWall_OSS" style="font-size:13px;font-weight:600">TheFlightWall OSS</a> by <a href="https://github.com/AxisNimble">AxisNimble</a> &#8212; the open-source flight wall project this firmware extends.</div><h4>Data Sources &amp; Libraries</h4><div style="display:grid;grid-template-columns:1fr 1fr;gap:6px 18px"><div><a href="https://opensky-network.org">OpenSky Network</a> &#8212; ADS-B state vectors</div><div><a href="https://flightaware.com/commercial/aeroapi">FlightAware AeroAPI</a> &#8212; flight enrichment</div><div><a href="https://github.com/Jxck-S/airline-logos">Jxck-S/airline-logos</a> &#8212; airline logo assets</div><div><a href="https://images.weserv.nl">images.weserv.nl</a> &#8212; image proxy &amp; resize</div><div><a href="https://github.com/Bodmer/TFT_eSPI">TFT_eSPI</a> by Bodmer &#8212; TFT display driver</div><div><a href="https://github.com/Bodmer/TJpg_Decoder">TJpg_Decoder</a> by Bodmer &#8212; JPEG rendering</div><div><a href="https://github.com/tzapu/WiFiManager">WiFiManager</a> by tzapu &#8212; WiFi provisioning</div><div><a href="https://arduinojson.org">ArduinoJson</a> by Beno&icirc;t Blanchon &#8212; JSON library</div></div></div>
</footer>
<div class="ack">The Flight Wall CYD Edition &#8212; open-source ESP32 flight radar. Built with Arduino &amp; PlatformIO.</div>
</div>
<script>
const $=id=>document.getElementById(id); let configLoaded=false;
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
 const f=screen.flight; $('tft').dataset.wide=screen.width>400; $('resolution').textContent=`${screen.width||'--'} x ${screen.height||'--'} physical TFT`;
 if(!f){$('sident').textContent='SEARCHING...';$('scount').textContent='0/0';$('sroute').textContent='--- - ---';$('sairline').textContent='';$('saircraft').textContent='';$('sline1').textContent='';$('sline2').textContent='';$('sbar').style.width='0';return;}
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
  const tag=f.enriched?'<span class="tag">ENRICHED</span>':'<span class="tag tag-adsb">ADS-B</span>';
  const r=f.enriched?`<div class="route">${esc(route(f))}</div><div class="city">${esc(val(f.origin_city,''))}${f.destination_city?' → '+esc(f.destination_city):''}</div>`:`<div class="route" style="font-size:15px;color:var(--muted)">${f.on_ground?'On ground':'Airborne'}</div><div class="city"></div>`;
  return `<article class="flight"><div class="flight-id"><h3>${esc(val(f.ident))}</h3>${tag}</div>${r}<div class="facts">${fact('Airline',f.airline||f.operator_icao)}${fact('Aircraft',f.aircraft_display||f.aircraft_code)}${fact('Distance',fixed(f.distance_km)+' km')}${fact('Bearing',fixed(f.bearing_deg,0)+' deg')}${fact('Altitude',f.baro_altitude_m!=null?Math.round(f.baro_altitude_m*3.28084)+' ft':null)}${fact('Geo alt',f.geo_altitude_m!=null?Math.round(f.geo_altitude_m*3.28084)+' ft':null)}${fact('Speed',f.velocity_mps!=null?Math.round(f.velocity_mps*3.6)+' km/h':null)}${fact('V/rate',f.vertical_rate_mps!=null?fixed(f.vertical_rate_mps)+' m/s':null)}${fact('Heading',f.heading_deg!=null?fixed(f.heading_deg,0)+' deg':null)}${fact('Departure',when(f.actual_out_epoch))}${fact('Arrival',when(f.estimated_in_epoch))}${fact('ICAO24',f.icao24)}${fact('Squawk',f.squawk)}${fact('Country',f.origin_country)}</div></article>`;
 }).join('');
}
function scrollCards(dir){$('flights').scrollBy({left:dir*290,behavior:'smooth'});}
function renderEvents(events){
 const el=$('events'), pinned=el.scrollHeight-el.scrollTop-el.clientHeight<35;
 el.innerHTML=events.length?events.slice().reverse().map(e=>`<div class="event">${esc(e)}</div>`).join(''):'<div class="empty">Waiting for a fetch cycle...</div>';
 if(pinned)el.scrollTop=0;
}
async function poll(){
 try{const d=await (await fetch('/api/live',{cache:'no-store'})).json();$('connection').textContent='Device live';$('clock').textContent=new Date().toLocaleTimeString();renderScreen(d.screen);renderFlights(d.flights);renderEvents(d.events);$('cardtime').textContent=d.last_fetch?`Last scan ${new Date(d.last_fetch*1000).toLocaleTimeString()}`:'';}catch(e){$('connection').textContent='Device unavailable';}
}
async function loadConfig(){
 try{const d=await(await fetch('/api/config',{cache:'no-store'})).json();['lat','lon','brightness','fetch_sec','cycle_sec'].forEach(k=>$(k).value=d[k]??'');$('radius').value=d.radius_km??'';$('oskyIdState').textContent=d.opensky_configured?'configured':'';$('oskyState').textContent=d.opensky_configured?'configured':'';$('aeroState').textContent=d.aero_configured?'configured':'';configLoaded=true;}catch(e){$('msg').textContent='Configuration unavailable';}
}
async function saveConfig(){
 const p={lat:parseFloat($('lat').value),lon:parseFloat($('lon').value),radius_km:Math.max(1,parseFloat($('radius').value)),fetch_sec:Math.max(10,parseInt($('fetch_sec').value)),cycle_sec:Math.max(1,parseInt($('cycle_sec').value)),brightness:Math.min(255,Math.max(0,parseInt($('brightness').value)))};
 if($('clear_osky').checked){p.osky_id='';p.osky_sec='';}else{if($('osky_id').value)p.osky_id=$('osky_id').value;if($('osky_sec').value)p.osky_sec=$('osky_sec').value;}
 if($('clear_aero').checked)p.aero_key=''; else if($('aero_key').value)p.aero_key=$('aero_key').value;
 $('save').disabled=true;try{const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(p)});$('msg').textContent=r.ok?'Configuration saved. Device rebooting...':'Save failed.';}catch(e){$('msg').textContent='Save failed: '+e;$('save').disabled=false;}
}
loadConfig();poll();setInterval(poll,1000);
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
  object["ident"] = flight.ident;
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

void WebUIServer::begin(const std::vector<FlightInfo> *flights, const CYDDisplay *display)
{
  _flights = flights;
  _display = display;
  _server.on("/", HTTP_GET, [this] { onRoot(); });
  _server.on("/api/config", HTTP_GET, [this] { onGetConfig(); });
  _server.on("/api/config", HTTP_POST, [this] { onPostConfig(); });
  _server.on("/api/live", HTTP_GET, [this] { onGetLive(); });
  _server.on("/api/logo", HTTP_GET, [this] { onGetLogo(); });
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

void WebUIServer::onRoot()
{
  _server.sendHeader("Cache-Control", "no-cache, no-store");
  _server.send_P(200, "text/html", HTML_PAGE);
}

void WebUIServer::onGetConfig()
{
  DynamicJsonDocument doc(512);
  doc["lat"] = RuntimeConfig::centerLat();
  doc["lon"] = RuntimeConfig::centerLon();
  doc["radius_km"] = RuntimeConfig::radiusKm();
  doc["fetch_sec"] = RuntimeConfig::fetchIntervalSec();
  doc["cycle_sec"] = RuntimeConfig::displayCycleSec();
  doc["brightness"] = RuntimeConfig::brightness();
  doc["opensky_configured"] =
      RuntimeConfig::openskyClientId().length() && RuntimeConfig::openskyClientSecret().length();
  doc["aero_configured"] = RuntimeConfig::aeroApiKey().length() > 0;

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
  if (doc.containsKey("brightness")) RuntimeConfig::setBrightness(doc["brightness"].as<uint8_t>());
  if (doc.containsKey("osky_id")) RuntimeConfig::setOpenskyClientId(doc["osky_id"].as<String>());
  if (doc.containsKey("osky_sec")) RuntimeConfig::setOpenskyClientSecret(doc["osky_sec"].as<String>());
  if (doc.containsKey("aero_key")) RuntimeConfig::setAeroApiKey(doc["aero_key"].as<String>());

  RuntimeConfig::save();
  _server.send(200, "application/json", "{\"ok\":true}");
  DBG_INFO("WebUI: config saved - reboot scheduled");
  _pendingReboot = true;
}

void WebUIServer::onGetLive()
{
  DynamicJsonDocument doc(32768);
  JsonObject screen = doc.createNestedObject("screen");
  screen["width"] = _display ? _display->width() : 0;
  screen["height"] = _display ? _display->height() : 0;
  const size_t total = _flights ? _flights->size() : 0;
  const size_t index = total && _display ? _display->currentFlightIndex() % total : 0;
  screen["total"] = total;
  screen["index"] = index;
  if (total)
    addFlight(screen.createNestedObject("flight"), (*_flights)[index]);

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
