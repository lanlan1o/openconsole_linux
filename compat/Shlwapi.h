#pragma once
// Minimal Shlwapi.h for the Linux port: only the handful of shell light-weight
// functions the engine actually uses.

#include <windows.h>

HRESULT PathCreateFromUrlW(LPCWSTR pszUrl, LPWSTR pszPath, _Inout_ DWORD* pcchPath, DWORD dwFlags);