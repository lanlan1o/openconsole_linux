// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Linux port: the ETW/TraceLogging backend of the original file is not
// available on Linux, so all diagnostic tracing functions here are no-ops.
// The sequence-trace helpers still retain their buffer management so that
// state machine behavior is otherwise unchanged.

#include "precomp.h"
#include "tracing.hpp"

using namespace Microsoft::Console::VirtualTerminal;

void ParserTracing::TraceStateChange(_In_z_ const wchar_t* /*name*/) const noexcept
{
}

void ParserTracing::TraceOnAction(_In_z_ const wchar_t* /*name*/) const noexcept
{
}

void ParserTracing::TraceOnExecute(const wchar_t /*wch*/) const noexcept
{
}

void ParserTracing::TraceOnExecuteFromEscape(const wchar_t /*wch*/) const noexcept
{
}

void ParserTracing::TraceOnEvent(_In_z_ const wchar_t* /*name*/) const noexcept
{
}

void ParserTracing::TraceCharInput(const wchar_t wch)
{
    AddSequenceTrace(wch);
}

void ParserTracing::AddSequenceTrace(const wchar_t wch)
{
    _sequenceTrace.push_back(wch);
}

void ParserTracing::DispatchSequenceTrace(const bool /*fSuccess*/) noexcept
{
    ClearSequenceTrace();
}

void ParserTracing::ClearSequenceTrace() noexcept
{
    _sequenceTrace.clear();
}

// NOTE: I'm expecting this to not be null terminated
void ParserTracing::DispatchPrintRunTrace(const std::wstring_view& /*string*/) const
{
}