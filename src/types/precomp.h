/* Linux port replacement for types/precomp.h.
   Original pulled in <windows.h>, <combaseapi.h>, <UIAutomation.h>, <objbase.h>
   for UIA types that are out of scope for the Linux port.
   Sources include it with #include "precomp.h", which resolves to this file. */
#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <intsafe.h>

// Transitive includes the real PCH would otherwise provide:
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Everything the engine headers expect from the Windows console types:
#include <til.h>