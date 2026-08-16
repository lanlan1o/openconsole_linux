// Linux port: verify TextBuffer::GetPlainText(CopyRequest) for copy.
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
#include "TerminalApi.hpp"

#include <cstdio>

int main()
{
    FrontendTerminalApi api;
    api.Feed(L"hello world\r\nsecond line\r\n");

    auto& buf = api.CurrentBuffer();

    // NOTE: end.x is EXCLUSIVE in CopyRequest/_RowCopyHelper ([begin, end)).
    auto dump = [&](const char* label, til::point beg, til::point end, bool block) {
        const TextBuffer::CopyRequest req{ buf, beg, end,
            /*blockSelection=*/block,
            /*includeLineBreak=*/true,
            /*trimTrailingWhitespace=*/true,
            /*formatWrappedRows=*/false,
            /*bufferCoordinates=*/false };
        const auto wtext = buf.GetPlainText(req);
        const auto utf8 = til::u16u8(wtext);
        std::printf("%-20s -> '", label);
        for (unsigned char c : utf8)
            std::printf(c >= 0x20 && c < 0x7f ? "%c" : "\\x%02x", c);
        std::printf("' (len=%zu)\n", utf8.size());
    };

    dump("hello (0,0)-(5,0)", til::point{ 0, 0 }, til::point{ 5, 0 }, false);    // "hello"
    dump("world (6,0)-(11,0)", til::point{ 6, 0 }, til::point{ 11, 0 }, false);  // "world"
    dump("2 lines (0,0)-(11,1)", til::point{ 0, 0 }, til::point{ 11, 1 }, false); // "hello world\nsecond line"
    dump("block (0,0)-(4,2)", til::point{ 0, 0 }, til::point{ 4, 2 }, true);     // "hell\nseco\n"
    dump("last col (0,0)-(80,0)", til::point{ 0, 0 }, til::point{ 80, 0 }, false); // "hello world" (clamped?)
    return 0;
}
