// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Linux port: minimal PathCreateFromUrlW. The Windows original lives in
// Shlwapi; here we decode "file://" URLs into POSIX filesystem paths, matching
// the semantics used by OSC 7 in the terminal (see AdaptDispatch.cpp).

#include "precomp.h"
#include <Shlwapi.h>

static int _hexNibble(const wchar_t c)
{
    if (c >= L'0' && c <= L'9')
    {
        return c - L'0';
    }
    if (c >= L'a' && c <= L'f')
    {
        return c - L'a' + 10;
    }
    if (c >= L'A' && c <= L'F')
    {
        return c - L'A' + 10;
    }
    return -1;
}

HRESULT PathCreateFromUrlW(LPCWSTR pszUrl, LPWSTR pszPath, _Inout_ DWORD* pcchPath, DWORD /*dwFlags*/)
{
    if (pszUrl == nullptr || pszPath == nullptr || pcchPath == nullptr)
    {
        return E_INVALIDARG;
    }

    const auto bufferSize = *pcchPath;
    std::wstring url{ pszUrl };
    std::wstring result;

    if (til::starts_with(url, L"file://"))
    {
        // file://[host]/path -- skip the scheme and optional empty host.
        auto pos = size_t{ 7 };
        if (pos < url.size() && url[pos] == L'/')
        {
            // file:///path  (empty host)
        }
        else
        {
            // file://host/path -- skip the host up to the next '/'
            const auto slash = url.find(L'/', pos);
            pos = (slash == std::wstring::npos) ? url.size() : slash;
        }

        while (pos < url.size())
        {
            const auto c = url[pos++];
            if (c == L'%' && pos + 1 < url.size())
            {
                const auto hi = _hexNibble(url[pos]);
                const auto lo = _hexNibble(url[pos + 1]);
                if (hi >= 0 && lo >= 0)
                {
                    result.push_back(static_cast<wchar_t>(hi * 16 + lo));
                    pos += 2;
                    continue;
                }
            }
            result.push_back(c);
        }
    }
    else
    {
        result = url;
    }

    if (result.size() + 1 > bufferSize)
    {
        return E_INSUFFICIENT_BUFFER;
    }

    ::wcscpy(pszPath, result.c_str());
    *pcchPath = gsl::narrow<DWORD>(result.size());
    return S_OK;
}