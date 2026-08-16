/* Linux port replacement for renderer/base/precomp.h.
   The original pulled in a full precompiled header for the renderer project;
   the Linux port pulls headers per-TU instead. */
#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <intsafe.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <til.h>
#include "feature_flags.h"