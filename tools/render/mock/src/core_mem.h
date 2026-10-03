#pragma once
#include <stdlib.h>
namespace PFMem { inline void* alloc(size_t n) { return calloc(1, n); } }
