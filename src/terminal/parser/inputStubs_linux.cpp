// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Linux port: stubs for the OneCore-safe input APIs. These map Windows virtual
// keys to scan codes / characters; the concepts don't exist on Linux so they
// return neutral values. The VT input engine only uses these when generating
// its own key events, which the Linux renderer doesn't synthesize.

#include "precomp.h"
#include "../../interactivity/inc/VtApiRedirection.hpp"

UINT OneCoreSafeMapVirtualKeyW(_In_ UINT uCode, _In_ UINT uMapType)
{
    return 0;
}

SHORT OneCoreSafeVkKeyScanW(_In_ WCHAR ch)
{
    return 0;
}

SHORT OneCoreSafeGetKeyState(_In_ int nVirtKey)
{
    return 0;
}

UINT GetDoubleClickTime()
{
    return 500;
}
