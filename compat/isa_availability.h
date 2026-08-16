// Minimal stand-in for the MSVC "isa_availability.h" used by Row.cpp to pick a
// SWAR fast path when the host CPU supports AVX2. On Linux we detect the same
// feature with __builtin_cpu_supports and expose the checkpoint levels that the
// engine compares against. The row fill loops use AVX2/SSE intrinsics directly,
// so we also bring those in (with the appropriate target ISA).
#pragma once

#ifdef __GNUC__
#pragma GCC target("avx2")
#endif

#include <immintrin.h>

#define __ISA_AVAILABLE_SSE2 1
#define __ISA_AVAILABLE_SSE4_2 2
#define __ISA_AVAILABLE_AVX 3
#define __ISA_AVAILABLE_AVX2 4
#define __ISA_AVAILABLE_AVX512 5

extern "C" inline int __isa_available = __builtin_cpu_supports("avx2") ? __ISA_AVAILABLE_AVX2 : __ISA_AVAILABLE_SSE4_2;