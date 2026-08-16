#pragma once
// IntSafe stub - safe size/pointer arithmetic helpers used by the engine.
#include <stdint.h>
#include <cstddef>

#ifdef __cplusplus
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS
#endif

inline int UIntAdd(unsigned int a, unsigned int b, unsigned int* result) { *result = a + b; return 0; }
inline int UIntSub(unsigned int a, unsigned int b, unsigned int* result) { *result = a - b; return 0; }
inline int UIntMul(unsigned int a, unsigned int b, unsigned int* result) { *result = a * b; return 0; }
inline int SIZETAdd(size_t a, size_t b, size_t* result) { *result = a + b; return 0; }
inline int SIZETSub(size_t a, size_t b, size_t* result) { *result = a - b; return 0; }
inline int SIZETMul(size_t a, size_t b, size_t* result) { *result = a * b; return 0; }

inline int SizeTAdd(size_t a, size_t b, size_t* result) { *result = a + b; return 0; }
inline int SizeTSub(size_t a, size_t b, size_t* result) { *result = a - b; return 0; }
inline int SizeTMul(size_t a, size_t b, size_t* result) { *result = a * b; return 0; }

inline int DWordAdd(unsigned long a, unsigned long b, unsigned long* result) { *result = a + b; return 0; }
inline int DWordMul(unsigned long a, unsigned long b, unsigned long* result) { *result = a * b; return 0; }

#define INTSAFE_E_ARITHMETIC_OVERFLOW 0x80070216L

inline int SizeTToInt(size_t a, int* result) { *result = static_cast<int>(a); return 0; }
inline int IntToSizeT(int a, size_t* result) { *result = static_cast<size_t>(a); return 0; }
inline int SizeTToUInt(size_t a, unsigned int* result) { *result = static_cast<unsigned int>(a); return 0; }
inline int UIntToSizeT(unsigned int a, size_t* result) { *result = static_cast<size_t>(a); return 0; }
inline int ULongToSizeT(unsigned long a, size_t* result) { *result = static_cast<size_t>(a); return 0; }
inline int SizeTToULong(size_t a, unsigned long* result) { *result = static_cast<unsigned long>(a); return 0; }

#ifndef SIZE_T_MAX
#define SIZE_T_MAX (~static_cast<size_t>(0))
#endif
#ifndef SIZE_MAX
#define SIZE_MAX SIZE_T_MAX
#endif
#ifndef SHORT_MAX
#define SHORT_MAX 0x7fff
#endif
