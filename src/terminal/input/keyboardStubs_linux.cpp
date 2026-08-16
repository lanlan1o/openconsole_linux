// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Linux port: the TerminalInput::KeyboardHelper translates Win32 virtual key
// codes + modifier state into characters via ToUnicodeEx/LoadKeyboardLayoutW.
// On Linux there is no window station or keyboard layout table, so these are
// tolerated stubs: ToUnicodeEx returns 0 (the engine falls back to the raw
// character event), and a pseudo-HKL is accepted everywhere. The SDL frontend
// can pre-translate characters with its own keymap before invoking the engine.

#include "precomp.h"

#include <cwctype>

extern "C" void* TestHook_TerminalInput_KeyboardLayout()
{
    return nullptr;
}

int ToUnicodeEx(const UINT /*wVirtKey*/,
                const UINT /*wScanCode*/,
                const BYTE* /*lpKeyState*/,
                LPWSTR /*pwszBuff*/,
                const int /*cchBuff*/,
                const UINT /*wFlags*/,
                const HKL /*dwhkl*/) noexcept
{
    return 0;
}

HKL LoadKeyboardLayoutW(const wchar_t*, const UINT) noexcept
{
    return reinterpret_cast<HKL>(static_cast<size_t>(1));
}

UINT MapVirtualKeyExW(const UINT uCode, const UINT uMapType, const HKL) noexcept
{
    // Identity mapping: pretend the virtual key equals the scan code. This is a
    // degraded but stable fallback for the Kitty "US base layout" lookup.
    return uCode;
}

HKL GetKeyboardLayout(const DWORD) noexcept
{
    return reinterpret_cast<HKL>(static_cast<size_t>(1));
}

HWND GetForegroundWindow() noexcept
{
    return nullptr;
}

DWORD GetWindowThreadProcessId(const HWND, DWORD* const processId) noexcept
{
    if (processId)
    {
        *processId = 0;
    }
    return 0;
}

int LCMapStringEx(const wchar_t* /*lpLocaleName*/,
                  const DWORD dwMapFlags,
                  const wchar_t* lpSrcStr,
                  int cchSrc,
                  wchar_t* lpDestStr,
                  const int cchDest,
                  void*, void*, const DWORD) noexcept
{
    if (!lpSrcStr || !lpDestStr || cchSrc < 0)
    {
        return 0;
    }
    if (cchDest < cchSrc)
    {
        return 0;
    }

    for (int i = 0; i < cchSrc; ++i)
    {
        const auto c = dwMapFlags & LCMAP_LOWERCASE ? std::towlower(lpSrcStr[i]) : std::towupper(lpSrcStr[i]);
        lpDestStr[i] = static_cast<wchar_t>(c);
    }
    return cchSrc;
}