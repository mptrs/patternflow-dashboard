#pragma once
#include <stdint.h>
#include "Arduino.h"
struct Preferences {
  bool begin(const char*, bool = false) { return false; }
  void end() {}
  template <class T> T get(const char*, T d) { return d; }
  bool isKey(const char*) { return false; }
  bool getBool(const char*, bool d) { return d; } uint8_t getUChar(const char*, uint8_t d) { return d; }
  int8_t getChar(const char*, int8_t d) { return d; } uint16_t getUShort(const char*, uint16_t d) { return d; }
  float getFloat(const char*, float d) { return d; } String getString(const char*, const char* d) { return d; }
  size_t getString(const char*, char* b, size_t) { b[0] = 0; return 0; }
  size_t getBytes(const char*, void*, size_t) { return 0; }
  template <class... A> size_t putBool(A...) { return 0; } template <class... A> size_t putUChar(A...) { return 0; }
  template <class... A> size_t putChar(A...) { return 0; } template <class... A> size_t putUShort(A...) { return 0; }
  template <class... A> size_t putFloat(A...) { return 0; } template <class... A> size_t putString(A...) { return 0; }
  template <class... A> size_t putBytes(A...) { return 0; } template <class... A> bool remove(A...) { return true; }
};
