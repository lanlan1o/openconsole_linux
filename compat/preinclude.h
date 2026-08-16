#pragma once
// glibc: make __GLIBC_PREREQ available to libstdc++'s os_defines.h (it is used
// from #if before any libc header has pulled <features.h> in this TU).
#include <features.h>
// Linux port: force-included before every TU to satisfy common STL deps.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <span>
#include <string_view>

// fmt - wchar formatter used by til::color::ToHexString etc.
#include <fmt/compile.h>
#include <fmt/xchar.h>

// GSL (vendored) - provides gsl::narrow_cast / gsl::at used across the engine.
#include <gsl/util>
#include <gsl/pointers>
#include <gsl/narrow>

// SAL must come before WIL (its annoation macros are used by wil decls).
#include "sal.h"

// WIL stub (vendored) - scope_exit, ResultException and HR macros.
#include "wil/wil.h"

// Chromium safe_math
#include <base/numerics/safe_math.h>

// Feature flags (from src/features.xml, all AlwaysEnabled).
#include "feature_flags.h"

// Upstream code refers to the standard library as `wistd` (the OSS "wistd"
// header aliases std). Provide the same alias for all TUs.
namespace wistd = std;
