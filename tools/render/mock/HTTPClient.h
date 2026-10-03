#pragma once
#include "Arduino.h"
#define HTTP_CODE_OK 200
struct HTTPClient { void setTimeout(int) {} template <class C> bool begin(C&, const char*) { return false; } int GET() { return -1; } String getString() { return ""; } void end() {} };
