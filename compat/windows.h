/* Minimal Windows API compatibility header for the OpenConsole-to-Linux port.

   Provides just enough of the Windows type/macro surface that the
   OpenConsole engine sources compile against, without pulling in the
   real Windows SDK.  This is intentionally tiny and grows only as
   needed by the sources actually being ported.
*/
#pragma once

// Emulate the classic windows.h "windef" include guard so guarded code paths
// in the vendored sources that reference COLORREF etc. still light up.
#ifndef _WINDEF_
#define _WINDEF_
#endif
#ifndef _WINCONTYPES_
#define _WINCONTYPES_
#endif

#include <cstdint>
#include <cstddef>
#include <cwchar>
#include <cwctype>
#include <string_view>
#include <string>

// --- primitive typedefs ---------------------------------------------------
// NOTE: Windows DWORD/ULONG/LONG are always 32-bit. On Linux LP64, `unsigned
// long` is 64-bit, so we must use `unsigned int`/`int` here or every DWORD-
// sized struct/ABI we touch would be wrong.
using byte = unsigned char;
using BOOL = int;
using BYTE = unsigned char;
using WORD = unsigned short;
using DWORD = unsigned int;
using DWORD_PTR = unsigned long;
using ULONG_PTR = unsigned long;
using LONG_PTR = long;
using HKL = void*;
using LONG = int;
using ULONG = unsigned int;
using SHORT = short;
using USHORT = unsigned short;
using CHAR = char;
using WCHAR = wchar_t;
using UCHAR = unsigned char;
using LPCWSTR = const wchar_t*;
using LPWSTR = wchar_t*;
using PCWSTR = const wchar_t*;
using PWSTR = wchar_t*;
using LPCSTR = const char*;
using LPSTR = char*;
using LPCOLORREF = DWORD*;
using COLORREF = unsigned int;
using SIZE_T = std::size_t;
using INT = int;
using UINT = unsigned int;
using LONGLONG = long long;
using ULONGLONG = unsigned long long;
using LPARAM = long;
using WPARAM = unsigned long;
using LRESULT = long;
using HRESULT = long;
using NTSTATUS = long;

// --- handles --------------------------------------------------------------
using HANDLE = void*;
using HWND = void*;
using PHANDLE = HANDLE*;
static const HANDLE INVALID_HANDLE_VALUE = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
constexpr int _ITERATOR_DEBUG_LEVEL = 0;

// __declspec is MSVC-specific; clang accepts __declspec for these spellings on
// e.g. Windows ABIs. On Linux, just map it away.
#ifndef __declspec
#define __declspec(x)
#endif
#ifndef __forceinline
#define __forceinline inline
#endif
#ifndef __assume
#define __assume(x) do { if (!(x)) { __builtin_unreachable(); } } while (0, 0)
#endif
#ifndef __pragma
#define __pragma(x)
#endif
#ifndef sealed
#define sealed
#endif
#ifndef abstract
#define abstract
#endif

// --- float/const pitfalls -------------------------------------------------
#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// NULL conforms to builtin; keep WINAPI-style calling conventions as no-ops
#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif

// --- HRESULT helpers ------------------------------------------------------
#define S_OK ((HRESULT)0L)
#define S_FALSE ((HRESULT)1L)
#define E_NOTIMPL ((HRESULT)0x80004001L)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define E_INVALIDARG ((HRESULT)0x80070057L)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_ABORT ((HRESULT)0x80004004L)
#define E_HANDLE ((HRESULT)0x80070006L)
#define E_INSUFFICIENT_BUFFER ((HRESULT)0x8007007AL)
#define E_ACCESSDENIED ((HRESULT)0x80070005L)
#define E_UNEXPECTED ((HRESULT)0x8000FFFFL)
#define E_NOT_VALID_STATE ((HRESULT)0x8007139FL)
#define CO_E_CLASSSTRING ((HRESULT)0x800401F3L)

#define HRESULT_FROM_WIN32(x) ((HRESULT)(x) <= 0 ? (HRESULT)(x) : (HRESULT)(((x) & 0x0000FFFF) | (7L << 16) | 0x80000000L))
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

// --- NTSTATUS helpers ------------------------------------------------------
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

// --- misc ----------------------------------------------------------------
inline thread_local unsigned long _g_lastError = 0;
inline unsigned long GetLastError() noexcept
{
    return _g_lastError;
}
inline void SetLastError(unsigned long err) noexcept
{
    _g_lastError = err;
}
#define THROW_LAST_ERROR_IF(cond)                 \
    do                                            \
    {                                             \
        if (cond)                                 \
        {                                         \
            THROW_HR(HRESULT_FROM_WIN32(GetLastError())); \
        }                                         \
    } while (0, 0)

#define FALSE 0
#define TRUE 1
#define CP_UTF8 65001
#define LOCALE_NAME_USER_DEFAULT L""
#define LINGUISTIC_IGNORECASE 0x00000010
#define NORM_IGNORECASE 0x00000001

// WideCharToMultiByte - UTF-16 -> UTF-8 (the only code page the engine asks for).
inline int WideCharToMultiByte(unsigned int /*codePage*/, unsigned long /*dwFlags*/,
                               const wchar_t* lpWideCharStr, int cchWideChar,
                               char* lpMultiByteStr, int cbMultiByte,
                               const char*, const void*)
{
    if (lpMultiByteStr == nullptr)
    {
        int byteLen = 0;
        const auto end = (cchWideChar < 0) ? static_cast<int>(wcslen(lpWideCharStr)) : cchWideChar;
        for (int i = 0; i < end; ++i)
        {
            const auto c = static_cast<unsigned int>(lpWideCharStr[i]);
            if (c < 0x80)
            {
                byteLen += 1;
            }
            else if (c < 0x800)
            {
                byteLen += 2;
            }
            else if (c < 0xD800 || c > 0xDFFF)
            {
                byteLen += 3;
            }
            else
            {
                // surrogate pair
                byteLen += 4;
                ++i;
            }
        }
        return byteLen;
    }

    auto* out = lpMultiByteStr;
    const auto end = (cchWideChar < 0) ? static_cast<int>(wcslen(lpWideCharStr)) : cchWideChar;
    int i = 0;
    int written = 0;
    while (i < end)
    {
        unsigned int cp = static_cast<unsigned int>(lpWideCharStr[i]);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < end)
        {
            const auto lo = static_cast<unsigned int>(lpWideCharStr[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF)
            {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
        if (cp < 0x80)
        {
            *out++ = static_cast<char>(cp);
        }
        else if (cp < 0x800)
        {
            *out++ = static_cast<char>(0xC0 | (cp >> 6));
            *out++ = static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            *out++ = static_cast<char>(0xE0 | (cp >> 12));
            *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *out++ = static_cast<char>(0x80 | (cp & 0x3F));
        }
        else
        {
            *out++ = static_cast<char>(0xF0 | (cp >> 18));
            *out++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *out++ = static_cast<char>(0x80 | (cp & 0x3F));
        }
        written = static_cast<int>(out - lpMultiByteStr);
        ++i;
    }
    if (written < cbMultiByte)
    {
        *out = 0;
    }
    return written;
}

inline int MultiByteToWideChar(unsigned int /*codePage*/, unsigned long /*dwFlags*/,
                               const char* lpMultiByteStr, int cbMultiByte,
                               wchar_t* lpWideCharStr, int cchWideChar)
{
    // Note: only handles UTF-8, which is the only code page the engine asks for.
    if (lpWideCharStr == nullptr)
    {
        // compute required length
        int wideLen = 0;
        const unsigned char* p = reinterpret_cast<const unsigned char*>(lpMultiByteStr);
        const int end = (cbMultiByte < 0) ? static_cast<int>(__builtin_strlen(lpMultiByteStr)) : cbMultiByte;
        for (int i = 0; i < end;)
        {
            const auto c = p[i];
            if (c < 0x80)
            {
                ++wideLen;
                ++i;
            }
            else if ((c & 0xE0) == 0xC0)
            {
                wideLen += 1;
                i += 2;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                wideLen += 1;
                i += 3;
            }
            else if ((c & 0xF8) == 0xF0)
            {
                wideLen += 2;
                i += 4;
            }
            else
            {
                ++wideLen;
                ++i;
            }
        }
        return wideLen;
    }

    auto* out = lpWideCharStr;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(lpMultiByteStr);
    const int end = (cbMultiByte < 0) ? static_cast<int>(__builtin_strlen(lpMultiByteStr)) : cbMultiByte;
    int i = 0;
    int written = 0;
    while (i < end)
    {
        const auto c = p[i];
        if (c < 0x80)
        {
            *out++ = static_cast<wchar_t>(c);
            ++i;
            ++written;
        }
        else if ((c & 0xE0) == 0xC0 && i + 1 < end)
        {
            *out++ = static_cast<wchar_t>(((c & 0x1F) << 6) | (p[i + 1] & 0x3F));
            i += 2;
            ++written;
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < end)
        {
            *out++ = static_cast<wchar_t>(((c & 0x0F) << 12) | ((p[i + 1] & 0x3F) << 6) | (p[i + 2] & 0x3F));
            i += 3;
            ++written;
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < end)
        {
            const auto cp = static_cast<unsigned int>(((c & 0x07) << 18) | ((p[i + 1] & 0x3F) << 12) | ((p[i + 2] & 0x3F) << 6) | (p[i + 3] & 0x3F));
            const auto cpMinus = cp - 0x10000;
            *out++ = static_cast<wchar_t>(0xD800 + (cpMinus >> 10));
            *out++ = static_cast<wchar_t>(0xDC00 + (cpMinus & 0x3FF));
            i += 4;
            written += 2;
        }
        else
        {
            *out++ = static_cast<wchar_t>(c);
            ++i;
            ++written;
        }
    }
    if (written < cchWideChar)
    {
        *out = 0;
    }
    return written;
}

inline int CompareStringEx(const void* /*localeName*/, unsigned long /*dwFlags*/,
                           const wchar_t* lpString1, int cchCount1,
                           const wchar_t* lpString2, int cchCount2,
                           void*, void*, unsigned long /*version*/)
{
    const auto lhsLen1 = (cchCount1 < 0) ? static_cast<int>(wcslen(lpString1)) : cchCount1;
    const auto rhsLen1 = (cchCount2 < 0) ? static_cast<int>(wcslen(lpString2)) : cchCount2;
    const auto n = lhsLen1 < rhsLen1 ? lhsLen1 : rhsLen1;
    int cmp = 0;
    for (int i = 0; i < n; ++i)
    {
        const auto a = std::towupper(lpString1[i]);
        const auto b = std::towupper(lpString2[i]);
        if (a != b)
        {
            cmp = a < b ? -1 : 1;
            break;
        }
    }
    if (cmp == 0)
    {
        cmp = (lhsLen1 == rhsLen1) ? 0 : (lhsLen1 < rhsLen1 ? -1 : 1);
    }
    return cmp + 2;
}

inline int FindNLSStringEx(const void* /*localeName*/, unsigned long /*dwFlags*/,
                           const wchar_t* lpStringSource, int cchSource,
                           const wchar_t* lpStringValue, int cchValue,
                           void*, void*, void*, unsigned long)
{
    const auto src = std::wstring_view(lpStringSource, cchSource);
    const auto val = std::wstring_view(lpStringValue, cchValue);
    std::wstring lowerSrc, lowerVal;
    lowerSrc.reserve(src.size());
    lowerVal.reserve(val.size());
    for (const auto c : src)
    {
        lowerSrc.push_back(std::towlower(c));
    }
    for (const auto c : val)
    {
        lowerVal.push_back(std::towlower(c));
    }
    const auto pos = lowerSrc.find(lowerVal);
    if (pos == std::wstring::npos)
    {
        return -1;
    }
    return static_cast<int>(pos);
}

// CompareStringOrdinal - nlsapi ordinal string comparison, used by til/string.h.
// TRUE == ignoreCase (case-insensitive), we do a codepoint fold here.
#include <cwctype>
inline int CompareStringOrdinal(const void* lhs, int lhsLen, const void* rhs, int rhsLen, BOOL /*bIgnoreCase*/)
{
    const auto* a = static_cast<const wchar_t*>(lhs);
    const auto* b = static_cast<const wchar_t*>(rhs);
    const auto n = lhsLen < rhsLen ? lhsLen : rhsLen;
    const auto len = (lhsLen < 0 || rhsLen < 0) ? -1 : n;
    int cmp = 0;
    if (len >= 0)
    {
        for (int i = 0; i < len; ++i)
        {
            const auto ca = std::towupper(a[i]);
            const auto cb = std::towupper(b[i]);
            if (ca != cb)
            {
                cmp = ca < cb ? -1 : 1;
                break;
            }
        }
    }
    else
    {
        const wchar_t* x = a;
        const wchar_t* y = b;
        while (*x && *y)
        {
            const auto cx = std::towupper(*x++);
            const auto cy = std::towupper(*y++);
            if (cx != cy)
            {
                cmp = cx < cy ? -1 : 1;
                break;
            }
        }
        if (cmp == 0)
        {
            cmp = (*x == *y) ? 0 : (*y ? -1 : 1);
        }
    }
    if (cmp == 0 && lhsLen != rhsLen)
    {
        cmp = lhsLen < rhsLen ? -1 : 1;
    }
    return cmp + 2; // 1 = less, 2 = equal, 3 = greater
}
#define FIELD_OFFSET(type, field) ((long)offsetof(type, field))
#define UNREFERENCED_PARAMETER(P) (P)

#pragma pack(push, 1)
#pragma pack(pop)

// wincon.h essentials used by the engine
struct COORD
{
    COORD() = default;
    constexpr COORD(short x, short y) noexcept :
        X(x), Y(y)
    {
    }
    short X;
    short Y;
};

struct GUID
{
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
};

struct OVERLAPPED
{
    unsigned long Internal;
    unsigned long InternalHigh;
    union
    {
        struct
        {
            unsigned long Offset;
            unsigned long OffsetHigh;
        };
        void* Pointer;
    };
    HANDLE hEvent;
};

struct SMALL_RECT
{
    SMALL_RECT() = default;
    constexpr SMALL_RECT(short l, short t, short r, short b) noexcept :
        Left(l), Top(t), Right(r), Bottom(b)
    {
    }
    short Left;
    short Top;
    short Right;
    short Bottom;
};

struct POINT
{
    POINT() = default;
    constexpr POINT(long x, long y) noexcept :
        x(x), y(y)
    {
    }
    long x;
    long y;
};

struct RECT
{
    RECT() = default;
    constexpr RECT(long l, long t, long r, long b) noexcept :
        left(l), top(t), right(r), bottom(b)
    {
    }
    long left;
    long top;
    long right;
    long bottom;
};

struct SIZE
{
    SIZE() = default;
    constexpr SIZE(long cx, long cy) noexcept :
        cx(cx), cy(cy)
    {
    }
    long cx;
    long cy;
};

struct KEY_EVENT_RECORD
{
    BOOL bKeyDown;
    WORD wRepeatCount;
    WORD wVirtualKeyCode;
    WORD wVirtualScanCode;
    union
    {
        wchar_t UnicodeChar;
        char AsciiChar;
    } uChar;
    DWORD dwControlKeyState;
};

struct MOUSE_EVENT_RECORD
{
    COORD dwMousePosition;
    DWORD dwButtonState;
    DWORD dwControlKeyState;
    DWORD dwEventFlags;
};

struct WINDOW_BUFFER_SIZE_RECORD
{
    COORD dwSize;
};

struct MENU_EVENT_RECORD
{
    UINT dwCommandId;
};

struct FOCUS_EVENT_RECORD
{
    BOOL bSetFocus;
};

struct RGBQUAD
{
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
};

struct CHAR_INFO
{
    union
    {
        wchar_t UnicodeChar;
        char AsciiChar;
    } Char;
    WORD Attributes;
};

struct INPUT_RECORD
{
    WORD EventType;
    union
    {
        KEY_EVENT_RECORD KeyEvent;
        MOUSE_EVENT_RECORD MouseEvent;
        WINDOW_BUFFER_SIZE_RECORD WindowBufferSizeEvent;
        MENU_EVENT_RECORD MenuEvent;
        FOCUS_EVENT_RECORD FocusEvent;
    } Event;
};

// winerror constants that sneak in
#define ERROR_UNHANDLED_EXCEPTION 574L
#define ERROR_IO_PENDING 997L
#define ERROR_INVALID_HANDLE 6L
#define ERROR_BROKEN_PIPE 109L
#define ERROR_INSUFFICIENT_BUFFER 122L
#define ERROR_NOT_ENOUGH_MEMORY 8L
#define ERROR_INVALID_PARAMETER 87L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_HANDLE_EOF 38L
#define ERROR_ARITHMETIC_OVERFLOW 534L
#define ERROR_INVALID_DATA 13L

#define _Count_of_(x) (sizeof(x) / sizeof((x)[0]))

// MSVC "secure" CRT functions that map 1:1 onto the standard ones on Linux.
#ifndef swprintf_s
#define swprintf_s swprintf
#endif

// VT cursor constants that sneak in from wincon.h
#define COLUMNS 80
#define ROWS 25

#define FOREGROUND_BLUE 0x0001
#define FOREGROUND_GREEN 0x0002
#define FOREGROUND_RED 0x0004
#define FOREGROUND_INTENSITY 0x0008
#define BACKGROUND_BLUE 0x0010
#define BACKGROUND_GREEN 0x0020
#define BACKGROUND_RED 0x0040
#define BACKGROUND_INTENSITY 0x0080
#define COMMON_LVB_LEADING_BYTE 0x0100
#define COMMON_LVB_TRAILING_BYTE 0x0200
#define COMMON_LVB_GRID_HORIZONTAL 0x0400
#define COMMON_LVB_GRID_LVERTICAL 0x0800
#define COMMON_LVB_GRID_RVERTICAL 0x1000
#define COMMON_LVB_SBCSDBCS 0x0200
#define COMMON_LVB_REVERSE_VIDEO 0x4000
#define COMMON_LVB_UNDERSCORE 0x8000

// COLORREF RGB extraction helpers (winuser.h / windef.h)
#define RGB(r, g, b) (COLORREF)(((BYTE)(r) & 0xFF) | (((BYTE)(g) & 0xFF) << 8) | (((BYTE)(b) & 0xFF) << 16) | 0x00000000u)
#define GetRValue(rgb) ((BYTE)(rgb))
#define GetGValue(rgb) ((BYTE)(((COLORREF)(rgb) >> 8) & 0xFF))
#define GetBValue(rgb) ((BYTE)(((COLORREF)(rgb) >> 16) & 0xFF))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | (((WORD)((BYTE)(b))) << 8)))
#define MAKELONG(a, b) ((long)(((WORD)(a)) | (((DWORD)((WORD)(b))) << 16)))

#ifndef IS_HIGH_SURROGATE
#define IS_HIGH_SURROGATE(wch) ((wch) >= 0xD800 && (wch) <= 0xDBFF)
#endif
#ifndef IS_LOW_SURROGATE
#define IS_LOW_SURROGATE(wch) ((wch) >= 0xDC00 && (wch) <= 0xDFFF)
#endif

// wincon keyboard/mouse flags (values are shared by the engine whitespace)
#define KEY_EVENT 0x0001
#define MOUSE_EVENT 0x0002
#define WINDOW_BUFFER_SIZE_EVENT 0x0004
#define MENU_EVENT 0x0008
#define FOCUS_EVENT 0x0010

// dwControlKeyState / dwButtonState flags (wincon.h)
#define CAPSLOCK_ON 0x0080
#define NUMLOCK_ON 0x0020
#define SCROLLLOCK_ON 0x0040
#define LEFT_CTRL_PRESSED 0x0008
#define RIGHT_CTRL_PRESSED 0x0004
#define LEFT_ALT_PRESSED 0x0002
#define RIGHT_ALT_PRESSED 0x0001
#define SHIFT_PRESSED 0x0010
#define CTRL_PRESSED (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)
#define ALT_PRESSED (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)
#define ENHANCED_KEY 0x0100
#define FROM_LEFT_1ST_BUTTON_PRESSED 0x0001
#define RIGHTMOST_BUTTON_PRESSED 0x0002
#define FROM_LEFT_2ND_BUTTON_PRESSED 0x0004
#define FROM_LEFT_3RD_BUTTON_PRESSED 0x0008
#define MOUSE_WHEELED 0x0004
#define MOUSE_HWHEELED 0x0008
#define DOUBLE_CLICK 0x0002
#define MOUSE_MOVED 0x0001

// Window messages (winuser.h) used by the input engine.
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MBUTTONDBLCLK 0x0209
#define WM_MOUSEWHEEL 0x020A
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_XBUTTONDBLCLK 0x020D
#define WM_MOUSEHWHEEL 0x020E

// Virtual key codes (winuser.h), only the subset the VT input engine needs.
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_MBUTTON 0x04
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_F13 0x7C
#define VK_F14 0x7D
#define VK_F15 0x7E
#define VK_F16 0x7F
#define VK_F17 0x80
#define VK_F18 0x81
#define VK_F19 0x82
#define VK_F20 0x83
#define VK_F21 0x84
#define VK_F22 0x85
#define VK_F23 0x86
#define VK_F24 0x87

#define WHEEL_DELTA 120

#define MAPVK_VK_TO_VSC 0
#define MAPVK_VSC_TO_VK 1
#define MAPVK_VK_TO_CHAR 2
#define MAPVK_VSC_TO_VK_EX 3
#define MAPVK_VK_TO_VSC_EX 4

#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif
#define WAIT_OBJECT_0 0x00000000
#define WAIT_TIMEOUT 0x00000102
#define WAIT_FAILED 0xFFFFFFFF

#define PSEUDOCONSOLE_RESIZE_QUIRK 0

UINT GetDoubleClickTime();
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#define ENABLE_WINDOW_INPUT 0x0008
#define ENABLE_MOUSE_INPUT 0x0010
#define ENABLE_QUICK_EDIT_MODE 0x0040
#define ENABLE_ECHO_INPUT 0x0004
#define ENABLE_LINE_INPUT 0x0002
#define ENABLE_PROCESSED_INPUT 0x0001
#define ENABLE_PROCESSED_OUTPUT 0x0001
#define ENABLE_WRAP_AT_EOL_OUTPUT 0x0002
// Additional virtual key codes used by the input engine.
#define VK_CANCEL 0x03
#define VK_CLEAR 0x0C
#define VK_PACKET 0xE7
#define VK_SNAPSHOT 0x2C
#define VK_APPS 0x5D
#define VK_LWIN 0x5B
#define VK_RWIN 0x5C
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SEPARATOR 0x6C
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91

// Media/volume keys (real names), used by terminalInput.cpp.
#define VK_MEDIA_PLAY_PAUSE 0xB3
#define VK_MEDIA_STOP 0xB2
#define VK_MEDIA_NEXT_TRACK 0xB0
#define VK_MEDIA_PREV_TRACK 0xB1
#define VK_VOLUME_DOWN 0xAE
#define VK_VOLUME_UP 0xAF
#define VK_VOLUME_MUTE 0xAD

// --- misc macros/types used by the terminal input engine -------------------
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
#define LOCALE_NAME_INVARIANT (L"")
#define LCMAP_LOWERCASE 0x00000100
#define LCMAP_UPPERCASE 0x00000200
#define LF_FACESIZE 32

// Keyboard layout / code conversion entry points. On Linux these are simple
// stubs (see src/terminal/input/keyboardStubs_linux.cpp): ToUnicodeEx returns
// 0 and HKLs are opaque pseudo-handles.
extern "C" void* TestHook_TerminalInput_KeyboardLayout();
int ToUnicodeEx(UINT wVirtKey, UINT wScanCode, const BYTE* lpKeyState, LPWSTR pwszBuff, int cchBuff, UINT wFlags, HKL dwhkl) noexcept;
HKL LoadKeyboardLayoutW(const wchar_t* pwszKLID, UINT Flags) noexcept;
UINT MapVirtualKeyExW(UINT uCode, UINT uMapType, HKL dwhkl) noexcept;
HKL GetKeyboardLayout(DWORD dwTid) noexcept;
HWND GetForegroundWindow() noexcept;
DWORD GetWindowThreadProcessId(HWND hWnd, DWORD* lpdwProcessId) noexcept;
int LCMapStringEx(const wchar_t* lpLocaleName, DWORD dwMapFlags, const wchar_t* lpSrcStr, int cchSrc, wchar_t* lpDestStr, int cchDest, void* lpVersionInformation, void* lpReserved, DWORD sortHandle) noexcept;

// --- virtual memory ---------------------------------------------------------
// The TextBuffer arena reserves with MEM_RESERVE and lazily MEM_COMMITs ROWs.
// On Linux a single malloc covers both: RESERVE allocates, COMMIT just touches
// the already-present pages (returns the same pointer), DECOMMIT is a no-op and
// the whole arena is freed by the owning unique_virtualalloc_ptr.
#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_DECOMMIT 0x00004000
#define MEM_RELEASE 0x00008000
#define PAGE_READWRITE 0x00000004

inline void* VirtualAlloc(void* lpAddress, size_t dwSize, DWORD flAllocationType, DWORD /*flProtect*/)
{
    if (flAllocationType & MEM_COMMIT)
    {
        return lpAddress;
    }
    if (dwSize == 0)
    {
        dwSize = 1;
    }
    if (auto p = std::malloc(dwSize))
    {
        return p;
    }
    SetLastError(static_cast<unsigned long>(errno));
    return nullptr;
}

inline void VirtualFree(void* lpAddress, const size_t /*dwSize*/, const DWORD flFreeType)
{
    if (flFreeType & MEM_RELEASE)
    {
        std::free(lpAddress);
    }
}

// WriteFile - used to serialize the buffer to a handle (POSIX fd stored in the
// HANDLE). Returns TRUE on success, FALSE otherwise.
inline bool WriteFile(HANDLE hFile, const void* lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten, void* /*lpOverlapped*/)
{
    const auto fd = static_cast<int>(reinterpret_cast<intptr_t>(hFile));
    const auto written = ::write(fd, lpBuffer, nNumberOfBytesToWrite);
    if (written < 0)
    {
        SetLastError(static_cast<unsigned long>(errno));
        if (lpNumberOfBytesWritten)
        {
            *lpNumberOfBytesWritten = 0;
        }
        return false;
    }
    if (lpNumberOfBytesWritten)
    {
        *lpNumberOfBytesWritten = static_cast<DWORD>(written);
    }
    return true;
}

#define ERROR_WRITE_FAULT 29

// Alphanumeric + OEM virtual key codes (winuser.h)
#define VK_0 0x30
#define VK_1 0x31
#define VK_2 0x32
#define VK_3 0x33
#define VK_4 0x34
#define VK_5 0x35
#define VK_6 0x36
#define VK_7 0x37
#define VK_8 0x38
#define VK_9 0x39
#define VK_A 0x41
#define VK_B 0x42
#define VK_C 0x43
#define VK_D 0x44
#define VK_E 0x45
#define VK_F 0x46
#define VK_G 0x47
#define VK_H 0x48
#define VK_I 0x49
#define VK_J 0x4A
#define VK_K 0x4B
#define VK_L 0x4C
#define VK_M 0x4D
#define VK_N 0x4E
#define VK_O 0x4F
#define VK_P 0x50
#define VK_Q 0x51
#define VK_R 0x52
#define VK_S 0x53
#define VK_T 0x54
#define VK_U 0x55
#define VK_V 0x56
#define VK_W 0x57
#define VK_X 0x58
#define VK_Y 0x59
#define VK_Z 0x5A
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE
#define VK_OEM_8 0xDF
#define VK_OEM_102 0xE2
