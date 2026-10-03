// Patternflow Dashboard - weather from Open-Meteo (free, no API key).
//
// The HTTPS fetch runs in its own short-lived task on core 0, so the panel
// never stalls while a TLS handshake takes its few seconds. The task fills
// `incoming`; the loop task (core 1) copies it to `current` at a frame
// boundary, and only the loop task ever reads `current`.
#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "dashboard_config.h"

namespace DashWeather {

constexpr int HOURS = 24;
constexpr int DAYS = 5;

struct Data {
  bool valid = false;
  float temp = NAN, feels = NAN, wind = NAN;
  int code = 0;
  bool isDay = true;
  int hours = 0;
  int8_t hHour[HOURS];
  float hTemp[HOURS];
  int8_t hRain[HOURS];
  uint8_t hCode[HOURS];
  bool hDay[HOURS];
  int days = 0;
  int16_t dYear[DAYS];
  int8_t dMonth[DAYS], dDay[DAYS];
  uint8_t dCode[DAYS];
  float dMax[DAYS], dMin[DAYS];
  int8_t dRain[DAYS];
  char sunrise[6] = "", sunset[6] = "";
};

// Settings (namespace "dashboard" in NVS, separate from Patternflow's own)
inline float lat = NAN, lon = NAN;
inline char place[40] = "";

inline Data current;       // read by the loop task only
inline Data incoming;      // written by the fetch task only
inline volatile bool incomingReady = false;
inline volatile bool fetching = false;
inline uint32_t nextFetchMs = 0;
inline uint32_t updatedAtMs = 0;
inline char lastError[48] = "";

inline bool hasLocation() { return !isnan(lat) && !isnan(lon); }

inline void loadSettings() {
  Preferences prefs;
  if (!prefs.begin("dashboard", true)) return;
  if (prefs.isKey("lat")) lat = prefs.getFloat("lat", NAN);
  if (prefs.isKey("lon")) lon = prefs.getFloat("lon", NAN);
  if (prefs.isKey("place")) prefs.getString("place", place, sizeof(place));
  prefs.end();
}

inline void saveSettings(float la, float lo, const char* name) {
  lat = la;
  lon = lo;
  strncpy(place, name ? name : "", sizeof(place) - 1);
  place[sizeof(place) - 1] = 0;
  Preferences prefs;
  if (!prefs.begin("dashboard", false)) return;
  prefs.putFloat("lat", lat);
  prefs.putFloat("lon", lon);
  prefs.putString("place", place);
  prefs.end();
}

// ---------------------------------------------------------------- parsing
// Open-Meteo's JSON is flat and predictable: find a section such as
// "hourly":{...}, then a key inside it. No JSON library needed.
struct Span {
  const char* begin;
  const char* end;
};

inline Span section(const char* body, const char* name) {
  char key[32];
  snprintf(key, sizeof key, "\"%s\":{", name);
  const char* b = strstr(body, key);
  if (!b) return {nullptr, nullptr};
  b += strlen(key);
  const char* e = strchr(b, '}');
  return {b, e ? e : b};
}

inline const char* findKey(Span s, const char* name) {
  if (!s.begin) return nullptr;
  char key[48];
  snprintf(key, sizeof key, "\"%s\":", name);
  const size_t n = strlen(key);
  for (const char* p = s.begin; p && p < s.end; p++) {
    p = strstr(p, key);
    if (!p || p >= s.end) return nullptr;
    return p + n;
  }
  return nullptr;
}

inline float number(Span s, const char* name, float fallback = NAN) {
  const char* p = findKey(s, name);
  if (!p || strncmp(p, "null", 4) == 0) return fallback;
  return strtof(p, nullptr);
}

// Parses a numeric array; null becomes NAN. Returns the count.
inline int numbers(Span s, const char* name, float* out, int max) {
  const char* p = findKey(s, name);
  if (!p || *p != '[') return 0;
  p++;
  int n = 0;
  while (*p && *p != ']' && n < max) {
    if (strncmp(p, "null", 4) == 0) {
      out[n++] = NAN;
      p += 4;
    } else {
      char* end;
      out[n++] = strtof(p, &end);
      if (end == p) break;
      p = end;
    }
    if (*p == ',') p++;
  }
  return n;
}

// Parses an array of strings, calls fn(index, string start) for each one.
template <typename Fn>
inline int strings(Span s, const char* name, int max, Fn fn) {
  const char* p = findKey(s, name);
  if (!p || *p != '[') return 0;
  int n = 0;
  while (n < max) {
    p = strchr(p, '"');
    if (!p || p >= s.end) break;
    fn(n++, p + 1);
    p = strchr(p + 1, '"');
    if (!p) break;
    p++;
    if (*p == ']') break;
  }
  return n;
}

inline bool parse(const char* body, Data& d) {
  d = Data();
  const Span cur = section(body, "current");
  const Span hourly = section(body, "hourly");
  const Span daily = section(body, "daily");
  if (!cur.begin || !hourly.begin || !daily.begin) return false;

  d.temp = number(cur, "temperature_2m");
  d.feels = number(cur, "apparent_temperature");
  d.wind = number(cur, "wind_speed_10m");
  d.code = (int)number(cur, "weather_code", 0);
  d.isDay = number(cur, "is_day", 1) != 0;

  float tmp[HOURS];
  d.hours = numbers(hourly, "temperature_2m", d.hTemp, HOURS);
  int n = numbers(hourly, "precipitation_probability", tmp, HOURS);
  for (int i = 0; i < HOURS; i++) d.hRain[i] = i < n && !isnan(tmp[i]) ? (int8_t)tmp[i] : 0;
  n = numbers(hourly, "weather_code", tmp, HOURS);
  for (int i = 0; i < HOURS; i++) d.hCode[i] = i < n && !isnan(tmp[i]) ? (uint8_t)tmp[i] : 0;
  n = numbers(hourly, "is_day", tmp, HOURS);
  for (int i = 0; i < HOURS; i++) d.hDay[i] = i < n ? tmp[i] != 0 : true;
  strings(hourly, "time", HOURS, [&](int i, const char* s) { d.hHour[i] = (int8_t)atoi(s + 11); });

  d.days = numbers(daily, "temperature_2m_max", d.dMax, DAYS);
  numbers(daily, "temperature_2m_min", d.dMin, DAYS);
  n = numbers(daily, "weather_code", tmp, DAYS);
  for (int i = 0; i < DAYS; i++) d.dCode[i] = i < n && !isnan(tmp[i]) ? (uint8_t)tmp[i] : 0;
  n = numbers(daily, "precipitation_probability_max", tmp, DAYS);
  for (int i = 0; i < DAYS; i++) d.dRain[i] = i < n && !isnan(tmp[i]) ? (int8_t)tmp[i] : 0;
  strings(daily, "time", DAYS, [&](int i, const char* s) {
    d.dYear[i] = (int16_t)atoi(s);
    d.dMonth[i] = (int8_t)atoi(s + 5);
    d.dDay[i] = (int8_t)atoi(s + 8);
  });
  strings(daily, "sunrise", 1, [&](int, const char* s) { memcpy(d.sunrise, s + 11, 5); d.sunrise[5] = 0; });
  strings(daily, "sunset", 1, [&](int, const char* s) { memcpy(d.sunset, s + 11, 5); d.sunset[5] = 0; });

  d.valid = !isnan(d.temp) && d.hours > 0 && d.days > 0;
  return d.valid;
}

// ---------------------------------------------------------------- fetching
inline void buildUrl(char* out, size_t n, bool secure) {
  snprintf(out, n,
           "%s://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&timezone=auto"
           "&forecast_days=%d&forecast_hours=%d"
           "&current=temperature_2m,apparent_temperature,weather_code,is_day,wind_speed_10m"
           "&hourly=temperature_2m,precipitation_probability,weather_code,is_day"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset",
           secure ? "https" : "http", lat, lon, DAYS, HOURS);
}

// HTTPS needs about 40 KB of internal RAM for TLS, and the panel's DMA buffers
// leave less than that on most boards. Open-Meteo answers plain HTTP too (the
// request carries only a city's coordinates), so TLS is used only when it fits.
constexpr size_t TLS_FREE = 96 * 1024, TLS_LARGEST = 48 * 1024;  // 61/43 KB was measured to fail
inline bool tlsFits() {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= TLS_FREE &&
         heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >= TLS_LARGEST;
}

inline void fetchTask(void* arg) {
  const bool secure = arg != nullptr;
  char url[512];
  buildUrl(url, sizeof url, secure);
  WiFiClientSecure tls;
  WiFiClient plain;
  if (secure) tls.setInsecure();  // same as Patternflow's own weather fetch: the board has no CA store
  HTTPClient http;
  http.setTimeout(8000);
  bool ok = false;
  if (secure ? http.begin(tls, url) : http.begin(plain, url)) {
    const int code = http.GET();
    if (code == HTTP_CODE_OK) {
      const String body = http.getString();
      ok = parse(body.c_str(), incoming);
      if (!ok) snprintf(lastError, sizeof lastError, "unexpected answer (%u bytes)", body.length());
    } else {
      snprintf(lastError, sizeof lastError, "HTTP %d", code);
    }
    http.end();
  } else {
    snprintf(lastError, sizeof lastError, "could not connect");
  }
  if (ok) {
    lastError[0] = 0;
    incomingReady = true;
  }
  Serial.printf("[DASH] weather (%s) %s%s\n", secure ? "https" : "http", ok ? "updated" : "failed: ", ok ? "" : lastError);
  fetching = false;
  vTaskDelete(nullptr);
}

inline void requestFetch(uint32_t afterMs = 0) { nextFetchMs = millis() + afterMs; }

// Called every frame from the feature's loop hook (core 1). Never blocks.
inline void tick() {
  if (incomingReady) {
    current = incoming;  // the fetch task is done with it
    incomingReady = false;
    updatedAtMs = millis();
    nextFetchMs = millis() + DashConfig::WEATHER_EVERY_MS;
  }
  if (fetching || !hasLocation() || WiFi.status() != WL_CONNECTED) return;
  if ((int32_t)(millis() - nextFetchMs) < 0) return;
  fetching = true;
  nextFetchMs = millis() + DashConfig::WEATHER_RETRY_MS;  // if this attempt fails
  const bool secure = tlsFits();
  if (xTaskCreatePinnedToCore(fetchTask, "dash_weather", secure ? 12288 : 8192, secure ? (void*)1 : nullptr, 1,
                              nullptr, 0) != pdPASS) {
    fetching = false;
    snprintf(lastError, sizeof lastError, "no memory for the fetch task");
  }
}

}  // namespace DashWeather
