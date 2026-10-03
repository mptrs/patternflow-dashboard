// Just enough of Arduino and the ESP32 SDK to compile the dashboard screens on a desktop.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define pgm_read_pointer(p) (*(p))
typedef void* TaskHandle_t;
typedef int BaseType_t;
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
inline void vTaskDelay(int) {}
inline void vTaskDelete(void*) {}
inline int xTaskCreatePinnedToCore(void (*)(void*), const char*, int, void*, int, TaskHandle_t*, int) { return 0; }
inline uint32_t mockMillis = 0;
inline uint32_t millis() { return mockMillis; }
struct String : std::string {
  using std::string::string;
  String() {}
  String(const std::string& s) : std::string(s) {}
  const char* c_str() const { return std::string::c_str(); }
  unsigned length() const { return (unsigned)size(); }
};
struct SerialMock { template <class... A> void printf(const char*, A...) {} void println(const char*) {} };
inline SerialMock Serial;
#define MALLOC_CAP_INTERNAL 0
inline size_t heap_caps_get_free_size(int) { return 0; }
inline size_t heap_caps_get_largest_free_block(int) { return 0; }
// The pattern API's input (unused by the renderer)
struct InputFrame { int knobDeltas[4] = {}; bool btnPressed[4] = {}; };
