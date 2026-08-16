// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// Linux port: the full renderer (see src/renderer/base/renderer.cpp upstream)
// drives a Direct2D/UIA paint pipeline. On Linux we keep the Renderer as a
// thin interface that the SDL grid frontend can implement; all the engine
// "trigger" methods are tolerated no-ops that the frontend polls instead.

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <span>

#include "../inc/RenderSettings.hpp"
#include "../inc/IRenderData.hpp"
#include "../../types/inc/Viewport.hpp"

namespace Microsoft::Console::Render
{
    enum class InhibitionSource
    {
        Client, // E.g. VT sequences
        Host, // E.g. because the window is out of focus
        User, // The user turned it off
    };

    class Renderer
    {
    public:
        Renderer(RenderSettings& renderSettings, IRenderData* pData) noexcept :
            _renderSettings(renderSettings),
            _renderData(pData)
        {
        }
        ~Renderer() = default;

        IRenderData* GetRenderData() const noexcept
        {
            return _renderData;
        }

        TimerHandle RegisterTimer(const char* /*description*/, TimerCallback /*routine*/)
        {
            return TimerHandle{ 1 };
        }
        bool IsTimerRunning(TimerHandle /*handle*/) const noexcept
        {
            return false;
        }
        TimerDuration GetTimerInterval(TimerHandle /*handle*/) const noexcept
        {
            return {};
        }
        void StartTimer(TimerHandle /*handle*/, TimerDuration /*delay*/)
        {
        }
        void StartRepeatingTimer(TimerHandle /*handle*/, TimerDuration /*interval*/)
        {
        }
        void StopTimer(TimerHandle /*handle*/)
        {
        }

        void NotifyPaintFrame() noexcept
        {
        }
        void SynchronizedOutputChanged() noexcept
        {
            _isStdoutLinefeedFocused = true;
        }
        void AllowCursorVisibility(InhibitionSource /*source*/, bool /*enable*/) noexcept
        {
        }
        void AllowCursorBlinking(InhibitionSource /*source*/, bool /*enable*/) noexcept
        {
        }
        void TriggerSystemRedraw(const til::rect* const /*prcDirtyClient*/) noexcept
        {
        }
        void TriggerRedraw(const Microsoft::Console::Types::Viewport& /*region*/) noexcept
        {
        }
        void TriggerRedraw(const til::point* const /*pcoord*/) noexcept
        {
        }
        void TriggerRedrawAll(const bool /*backgroundChanged*/ = false, const bool /*frameChanged*/ = false) noexcept
        {
        }
        void TriggerTitleChange() noexcept
        {
        }
        void TriggerSelection() noexcept
        {
        }
        void TriggerScroll() noexcept
        {
        }
        void TriggerScroll(const til::point* const /*pcoordDelta*/) noexcept
        {
        }
        void TriggerNewTextNotification(const std::wstring_view /*newText*/) noexcept
        {
        }
        void UpdateSoftFont(const std::span<const uint16_t> /*bitPattern*/,
                            const til::size /*cellSize*/,
                            const size_t /*centeringHint*/)
        {
        }
        void SetBackgroundOpacity(const double /*opacity*/) noexcept
        {
        }

    private:
        RenderSettings& _renderSettings;
        IRenderData* _renderData;
        bool _isStdoutLinefeedFocused{ false };
    };
}