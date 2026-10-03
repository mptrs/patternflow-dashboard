#pragma once
#include <stdint.h>
struct File { explicit operator bool() const { return false; } size_t read(uint8_t*, size_t) { return 0; } void close() {} bool isDirectory() { return false; }
  const char* name() { return ""; } File openNextFile() { return {}; } bool seek(uint32_t) { return false; } uint32_t size() { return 0; } };
struct FFatMock { File open(const char*, const char* = "r") { return {}; } bool exists(const char*) { return false; } bool mkdir(const char*) { return false; }
  bool remove(const char*) { return false; } bool rename(const char*, const char*) { return false; } };
inline FFatMock FFat;
#define FILE_READ "r"
#define FILE_WRITE "w"
