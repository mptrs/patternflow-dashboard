// Patternflow Dashboard - settings that are fixed at build time.
// (Settings you change on the device will get their own page later.)
#pragma once

// Home timezone as a POSIX TZ string. Default: Central European Time with
// EU daylight saving (Amsterdam, Berlin, Paris, ...).
#ifndef DASH_TZ
#define DASH_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
#endif
