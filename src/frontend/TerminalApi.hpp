// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Linux port frontend: ITerminalApi wired to the VT engine (AdaptDispatch owns
// a PageManager; the API provides the "visible" TextBuffer (main/alt) that both
// the engine and the SDL grid renderer use).

#pragma once

#include "til.h"
#include "intsafe.h"
#include "buffer/out/textBuffer.hpp"
#include "renderer/base/renderer.hpp"
#include "renderer/inc/RenderSettings.hpp"
#include "terminal/adapter/adaptDispatch.hpp"
#include "terminal/adapter/DispatchTypes.hpp"
#include "terminal/adapter/ITerminalApi.hpp"
#include "terminal/input/terminalInput.hpp"
#include "terminal/parser/OutputStateMachineEngine.hpp"
#include "terminal/parser/stateMachine.hpp"

#include <functional>
#include <memory>

class FrontendTerminalApi final : public Microsoft::Console::VirtualTerminal::ITerminalApi
{
public:
    using StateMachine = Microsoft::Console::VirtualTerminal::StateMachine;
    using Renderer = Microsoft::Console::Render::Renderer;
    using RenderSettings = Microsoft::Console::Render::RenderSettings;

    FrontendTerminalApi() noexcept :
        _renderer{ _renderSettings, nullptr },
        _mainBuffer{ til::size{ 80, 24 }, TextAttribute{}, 25, true, &_renderer },
        _viewport{ til::rect{ til::size{ 80, 24 } } }
    {
        auto dispatch = std::make_unique<Microsoft::Console::VirtualTerminal::AdaptDispatch>(*this, &_renderer, _renderSettings, _terminalInput);
        _stateMachine = std::make_unique<StateMachine>(std::make_unique<Microsoft::Console::VirtualTerminal::OutputStateMachineEngine>(std::move(dispatch)));
    }

    // ---- engine entry points (used by the frontend loop) -------------------

    void Feed(const std::wstring_view text)
    {
        _stateMachine->ProcessString(text);
    }

    TextBuffer& CurrentBuffer() noexcept
    {
        return _currentBuffer();
    }

    Microsoft::Console::VirtualTerminal::TerminalInput& Input() noexcept
    {
        return _terminalInput;
    }

    RenderSettings& Settings() noexcept
    {
        return _renderSettings;
    }

    const til::rect& ViewportRect() const noexcept
    {
        return _viewport;
    }

    // Called by the frontend when the window changes size.
    void Resize(const til::size newSize)
    {
        if (_mainBuffer.GetSize().Dimensions() != newSize)
        {
            _mainBuffer.ResizeTraditional(newSize);
        }
        if (_altBuffer)
        {
            _altBuffer->ResizeTraditional(newSize);
        }
        _viewport = til::rect{ newSize };
    }

    void SetWriteCallback(std::function<void(const std::string_view)> writer)
    {
        _writer = std::move(writer);
    }

    void SetTitleCallback(std::function<void(const std::wstring_view)> setter)
    {
        _titleSetter = std::move(setter);
    }

    bool ConsumeResizeRequested(til::size& out) noexcept
    {
        if (_resizeRequested)
        {
            out = _pendingResize;
            _resizeRequested = false;
            return true;
        }
        return false;
    }

    // ---- ITerminalApi ------------------------------------------------------

    void UnknownSequence() noexcept override
    {
    }

    void ReturnResponse(const std::wstring_view response) override
    {
        _Respond(response);
    }

    bool IsConPTY() const noexcept override
    {
        return false;
    }

    StateMachine& GetStateMachine() override
    {
        return *_stateMachine;
    }

    BufferState GetBufferAndViewport() override
    {
        return BufferState{ _currentBuffer(), _viewport, !_altActive };
    }

    void SetViewportPosition(const til::point position) override
    {
        _viewport.left = position.x;
        _viewport.top = position.y;
        if (_viewport.right < _viewport.left + 1)
        {
            _viewport.right = _viewport.left + _mainBuffer.GetSize().Width();
        }
        if (_viewport.bottom < _viewport.top + 1)
        {
            _viewport.bottom = _viewport.top + _mainBuffer.GetSize().Height();
        }
    }

    bool IsVtInputEnabled() const override
    {
        return true;
    }

    void SetSystemMode(const Mode mode, const bool enabled) override
    {
        if (enabled)
        {
            _systemModes.set(static_cast<size_t>(mode));
        }
        else
        {
            _systemModes.reset(static_cast<size_t>(mode));
        }
    }

    bool GetSystemMode(const Mode mode) const override
    {
        return _systemModes.test(static_cast<size_t>(mode));
    }

    void ReturnAnswerback() override
    {
        _Respond(L"\x1b[0c");
    }

    void WarningBell() override
    {
        if (_bell)
        {
            _bell();
        }
    }

    void SetWindowTitle(const std::wstring_view title) override
    {
        if (_titleSetter)
        {
            _titleSetter(title);
        }
    }

    void UseAlternateScreenBuffer(const TextAttribute& attrs) override
    {
        if (!_altBuffer)
        {
            _altBuffer = std::make_unique<TextBuffer>(_mainBuffer.GetSize().Dimensions(), attrs, 25, false, &_renderer);
        }
        else
        {
            _altBuffer->Reset();
            _altBuffer->SetCurrentAttributes(attrs);
        }
        _altActive = true;
    }

    void UseMainScreenBuffer() override
    {
        _altActive = false;
    }

    CursorType GetUserDefaultCursorStyle() const override
    {
        return CursorType::Legacy;
    }

    void ShowWindow(bool /*showOrHide*/) override
    {
    }

    void SetCodePage(const unsigned int /*codepage*/) override
    {
    }

    void ResetCodePage() override
    {
    }

    unsigned int GetOutputCodePage() const override
    {
        return 65001; // CP_UTF8
    }

    unsigned int GetInputCodePage() const override
    {
        return 65001; // CP_UTF8
    }

    void CopyToClipboard(const wil::zwstring_view /*content*/) override
    {
    }

    void SetTaskbarProgress(const Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState /*state*/, const size_t /*progress*/) override
    {
    }

    void SetWorkingDirectory(const std::wstring_view /*uri*/) override
    {
    }

    void PlayMidiNote(const int /*noteNumber*/, const int /*velocity*/, const std::chrono::microseconds /*duration*/) override
    {
    }

    bool ResizeWindow(const til::CoordType width, const til::CoordType height) override
    {
        _pendingResize = til::size{ width, height };
        _resizeRequested = true;
        return true;
    }

    void NotifyBufferRotation(const int /*delta*/) override
    {
    }

    void NotifyShellIntegrationMark() override
    {
    }

    void InvokeCompletions(std::wstring_view /*menuJson*/, unsigned int /*replaceLength*/) override
    {
    }

    void SearchMissingCommand(const std::wstring_view /*command*/) override
    {
    }

    void ShowNotification(const std::wstring_view /*title*/, const std::wstring_view /*body*/) override
    {
    }

    til::size GetCellSize() const noexcept override
    {
        return _cellSize;
    }

    void SetCellSize(til::size cellSize) noexcept
    {
        _cellSize = cellSize;
    }

private:
    TextBuffer& _currentBuffer() noexcept
    {
        return _altActive && _altBuffer ? *_altBuffer : _mainBuffer;
    }

    void _Respond(const std::wstring_view response)
    {
        if (_writer)
        {
            _writer(til::u16u8(response));
        }
    }

    RenderSettings _renderSettings;
    Renderer _renderer;
    Microsoft::Console::VirtualTerminal::TerminalInput _terminalInput;
    TextBuffer _mainBuffer;
    std::unique_ptr<TextBuffer> _altBuffer;
    bool _altActive = false;
    til::rect _viewport;
    std::unique_ptr<StateMachine> _stateMachine;
    std::bitset<3> _systemModes;
    std::function<void(const std::string_view)> _writer;
    std::function<void(const std::wstring_view)> _titleSetter;
    std::function<void()> _bell;
    bool _resizeRequested = false;
    til::size _pendingResize;
    til::size _cellSize{ 8, 16 };

public:
    void SetBellCallback(std::function<void()> bell)
    {
        _bell = std::move(bell);
    }
};