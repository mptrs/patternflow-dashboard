// Patternflow Dashboard - the settings page at http://patternflow.local/dashboard
// The browser looks the place up (Open-Meteo geocoding); the panel only stores
// the coordinates. Nothing personal is ever compiled into the firmware.
#pragma once
#include <Arduino.h>
#include "../../src/core_patterns_http.h"
#include "dash_weather.h"

namespace DashHttp {

inline WebServer& server() { return PatternflowPatternsHttp::server(); }

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Dashboard</title>
<style>body{font:16px system-ui,sans-serif;background:#111;color:#eee;max-width:28rem;margin:2rem auto;padding:0 1rem}
input,button{font:inherit;padding:.5rem;border-radius:.4rem;border:1px solid #444;background:#222;color:#eee}
button{background:#2a6;border:0;cursor:pointer}ul{list-style:none;padding:0}li{margin:.3rem 0}
li button{width:100%;text-align:left;background:#222;border:1px solid #444}.muted{color:#999}</style></head><body>
<h1>Dashboard</h1><p class="muted">Location for the weather screens.</p>
<p>Now: <b id="now">…</b></p>
<form id="f"><input id="q" placeholder="City, e.g. Utrecht" required> <button>Search</button></form>
<ul id="r"></ul><p id="msg" class="muted"></p>
<script>
const $=id=>document.getElementById(id);
async function status(){const s=await (await fetch('/api/dashboard')).json();
 $('now').textContent=s.place?`${s.place} (${s.lat.toFixed(3)}, ${s.lon.toFixed(3)})`:'not set';
 $('msg').textContent=s.error?`Last weather fetch: ${s.error}`:(s.updated?`Weather updated ${s.updated} s ago`:'');}
$('f').onsubmit=async e=>{e.preventDefault();$('r').innerHTML='';
 const j=await (await fetch('https://geocoding-api.open-meteo.com/v1/search?count=5&name='+encodeURIComponent($('q').value))).json();
 for(const p of j.results||[]){const b=document.createElement('button');const name=[p.name,p.admin1,p.country_code].filter(Boolean).join(', ');
  b.textContent=name;b.onclick=async()=>{await fetch('/api/dashboard',{method:'POST',body:new URLSearchParams({lat:p.latitude,lon:p.longitude,place:p.name})});
  $('r').innerHTML='';status();};const li=document.createElement('li');li.append(b);$('r').append(li);}
 if(!(j.results||[]).length)$('r').textContent='Nothing found.';};
status();setInterval(status,5000);
</script></body></html>)HTML";

inline void handlePage() {
  server().sendHeader("Cache-Control", "no-store");
  server().send_P(200, "text/html", PAGE);
}

inline void handleGet() {
  char json[200];
  const bool has = DashWeather::hasLocation();
  const unsigned age = DashWeather::updatedAtMs ? (millis() - DashWeather::updatedAtMs) / 1000 : 0;
  snprintf(json, sizeof json, "{\"place\":\"%s\",\"lat\":%.4f,\"lon\":%.4f,\"updated\":%u,\"error\":\"%s\"}",
           has ? DashWeather::place : "", has ? DashWeather::lat : 0.0f, has ? DashWeather::lon : 0.0f,
           DashWeather::updatedAtMs ? age : 0, DashWeather::lastError);
  server().sendHeader("Cache-Control", "no-store");
  server().send(200, "application/json", json);
}

inline void handlePost() {
  if (!server().hasArg("lat") || !server().hasArg("lon")) {
    server().send(400, "application/json", "{\"error\":\"lat and lon required\"}");
    return;
  }
  const float la = server().arg("lat").toFloat(), lo = server().arg("lon").toFloat();
  if (la < -90 || la > 90 || lo < -180 || lo > 180) {
    server().send(400, "application/json", "{\"error\":\"out of range\"}");
    return;
  }
  // Keep only characters the panel can draw and JSON can carry unescaped.
  String name = server().arg("place");
  String clean;
  for (size_t i = 0; i < name.length() && clean.length() < 38; i++) {
    const char c = name[i];
    if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\') clean += c;
  }
  DashWeather::saveSettings(la, lo, clean.c_str());
  DashWeather::requestFetch();
  handleGet();
}

inline void registerRoutes() {
  server().on("/dashboard", HTTP_GET, handlePage);
  server().on("/api/dashboard", HTTP_GET, handleGet);
  server().on("/api/dashboard", HTTP_POST, handlePost);
}

}  // namespace DashHttp
