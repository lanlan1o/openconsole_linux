// Linux port: standalone test for TerminalInput::HandleKey.
// Builds without Qt; links the engine libs. Prints what the engine would
// emit to the PTY for a given key event under various input modes.

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

#include <cstdio>
#include <memory>
#include <string>

namespace
{
    // Minimal ITerminalApi so AdaptDispatch + StateMachine can be constructed.
    // We only care about feeding shell-originated mode-setup sequences and then
    // querying HandleKey output, so most methods are stubs.
    class TestApi final : public Microsoft::Console::VirtualTerminal::ITerminalApi
    {
    public:
        TestApi() noexcept :
            _renderer{ _renderSettings, nullptr },
            _mainBuffer{ til::size{ 80, 24 }, TextAttribute{}, 25, true, &_renderer },
            _viewport{ til::rect{ til::size{ 80, 24 } } }
        {
            auto dispatch = std::make_unique<Microsoft::Console::VirtualTerminal::AdaptDispatch>(
                *this, &_renderer, _renderSettings, _terminalInput);
            _stateMachine = std::make_unique<Microsoft::Console::VirtualTerminal::StateMachine>(
                std::make_unique<Microsoft::Console::VirtualTerminal::OutputStateMachineEngine>(std::move(dispatch)));
        }

        void Feed(const std::wstring_view text) { _stateMachine->ProcessString(text); }

        Microsoft::Console::VirtualTerminal::TerminalInput& Input() noexcept { return _terminalInput; }

        // ---- ITerminalApi (stubs) ----
        void UnknownSequence() noexcept override {}
        void ReturnResponse(const std::wstring_view response) override { _lastResponse = std::wstring{ response }; }
        bool IsConPTY() const noexcept override { return false; }
        Microsoft::Console::VirtualTerminal::StateMachine& GetStateMachine() override { return *_stateMachine; }
        Microsoft::Console::VirtualTerminal::ITerminalApi::BufferState GetBufferAndViewport() override
        {
            return BufferState{ _mainBuffer, _viewport, true };
        }
        void SetViewportPosition(const til::point) override {}
        bool IsVtInputEnabled() const override { return true; }
        void SetSystemMode(const Mode mode, const bool enabled) override {}
        bool GetSystemMode(const Mode mode) const override { return false; }
        void ReturnAnswerback() override {}
        void WarningBell() override {}
        void SetWindowTitle(const std::wstring_view) override {}
        void UseAlternateScreenBuffer(const TextAttribute&) override {}
        void UseMainScreenBuffer() override {}
        CursorType GetUserDefaultCursorStyle() const override { return CursorType::Legacy; }
        void ShowWindow(bool) override {}
        void SetCodePage(const unsigned int) override {}
        void ResetCodePage() override {}
        unsigned int GetOutputCodePage() const override { return 65001; }
        unsigned int GetInputCodePage() const override { return 65001; }
        void CopyToClipboard(const wil::zwstring_view) override {}
        void SetTaskbarProgress(const Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState, const size_t) override {}
        void SetWorkingDirectory(const std::wstring_view) override {}
        void PlayMidiNote(const int, const int, const std::chrono::microseconds) override {}
        bool ResizeWindow(const til::CoordType, const til::CoordType) override { return false; }
        void NotifyBufferRotation(const int) override {}
        void NotifyShellIntegrationMark() override {}
        void InvokeCompletions(std::wstring_view, unsigned int) override {}
        void SearchMissingCommand(const std::wstring_view) override {}
        void ShowNotification(const std::wstring_view, const std::wstring_view) override {}
        til::size GetCellSize() const noexcept override { return { 8, 16 }; }

    private:
        Microsoft::Console::Render::RenderSettings _renderSettings;
        Microsoft::Console::Render::Renderer _renderer;
        Microsoft::Console::VirtualTerminal::TerminalInput _terminalInput;
        TextBuffer _mainBuffer;
        til::rect _viewport;
        std::unique_ptr<Microsoft::Console::VirtualTerminal::StateMachine> _stateMachine;
        std::wstring _lastResponse;
    };

    void PrintOutput(const char* label, const Microsoft::Console::VirtualTerminal::TerminalInput::OutputType& out)
    {
        std::printf("  %-32s: ", label);
        if (!out)
        {
            std::printf("(no output)\n");
            return;
        }
        for (wchar_t c : *out)
        {
            if (c >= 0x20 && c < 0x7f)
                std::printf("%c", static_cast<char>(c));
            else
                std::printf("\\x%04x", static_cast<unsigned>(c));
        }
        std::printf("\n");
    }

    Microsoft::Console::VirtualTerminal::TerminalInput::OutputType PressL(TestApi& api, wchar_t ch = L'l', uint16_t vk = 'L')
    {
        INPUT_RECORD rec{};
        rec.EventType = KEY_EVENT;
        rec.Event.KeyEvent.bKeyDown = TRUE;
        rec.Event.KeyEvent.wRepeatCount = 1;
        rec.Event.KeyEvent.wVirtualKeyCode = vk;
        rec.Event.KeyEvent.uChar.UnicodeChar = ch;
        rec.Event.KeyEvent.dwControlKeyState = 0;
        return api.Input().HandleKey(rec);
    }
}

int main()
{
    TestApi api;

    auto press = [](TestApi& a, uint16_t vk, wchar_t ch, DWORD cks) {
        INPUT_RECORD rec{};
        rec.EventType = KEY_EVENT;
        rec.Event.KeyEvent.bKeyDown = TRUE;
        rec.Event.KeyEvent.wRepeatCount = 1;
        rec.Event.KeyEvent.wVirtualKeyCode = vk;
        rec.Event.KeyEvent.uChar.UnicodeChar = ch;
        rec.Event.KeyEvent.dwControlKeyState = cks;
        return a.Input().HandleKey(rec);
    };

    PrintOutput("l  (cks=0)", press(api, 'L', L'l', 0));
    PrintOutput("a  (cks=0)", press(api, 'A', L'a', 0));
    PrintOutput("5  (cks=0)", press(api, '5', L'5', 0));
    PrintOutput("sp (cks=0)", press(api, VK_SPACE, L' ', 0));
    PrintOutput("l  (cks=ALT)", press(api, 'L', L'l', ALT_PRESSED));
    PrintOutput("l  (cks=CTRL)", press(api, 'L', L'l', CTRL_PRESSED));
    PrintOutput("Left (cks=0)", press(api, VK_LEFT, 0, 0));
    PrintOutput("Enter(cks=0)", press(api, VK_RETURN, L'\r', 0));

    return 0;
}
