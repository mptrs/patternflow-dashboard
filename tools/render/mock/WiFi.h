#pragma once
#define WL_CONNECTED 3
struct WiFiMock { int status() { return 0; } }; inline WiFiMock WiFi;
struct WiFiClient {};
