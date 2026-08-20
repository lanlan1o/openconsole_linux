cmake_minimum_required(VERSION 3.20)
project(openconsole_linux LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Use less opti. to support older cpus
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS_RELEASE "-O0 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=x86-64")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unused-parameter -Wno-missing-field-initializers -Wno-changes-meaning -include ${CMAKE_CURRENT_SOURCE_DIR}/compat/preinclude.h")

# Compat layer: provides windows.h / wil / sal / intsafe stubs
# it must come FIRST in the include path so that #include <windows.h> etc. resolve here.
set(COMPAT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/compat)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(OSS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/oss)

# Chromium safe_math + interval tree + fmt + gsl are vendored at repo root (oss/ + compat/gsl)
include_directories(
    ${COMPAT_DIR}
    ${COMPAT_DIR}/compat
    ${SRC_DIR}
    ${SRC_DIR}/inc
    ${SRC_DIR}/til
    ${SRC_DIR}/buffer/out
    ${SRC_DIR}/buffer/inc
    ${OSS_DIR}
    ${OSS_DIR}/chromium
    ${OSS_DIR}/interval_tree
    ${OSS_DIR}/../interval_tree
    ${OSS_DIR}/chromium/base
)

# Any directory that the forced preinclude.h needs must also reach compiler
# probes (e.g. TryRun/TryCompile like FindThreads), which inherit flags but
# not include_directories.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${COMPAT_DIR} -I${COMPAT_DIR}/compat -I${SRC_DIR}/inc -I${SRC_DIR}/til -I${OSS_DIR} -I${OSS_DIR}/chromium -I${OSS_DIR}/chromium/base")

# ------------------------------------------------------------------
# Libraries
# ------------------------------------------------------------------

# til: terminal implementation library (mostly header-only, plus precomp.cpp)
add_library(til STATIC
    ${SRC_DIR}/til/precomp_linux.cpp
)
target_include_directories(til PUBLIC ${SRC_DIR}/inc ${SRC_DIR}/til ${COMPAT_DIR})

# types: Viewport, color table, glyph width, utils, etc.
add_library(types STATIC
    ${SRC_DIR}/types/precomp_linux.cpp
    ${SRC_DIR}/types/CodepointWidthDetector.cpp
    ${SRC_DIR}/types/ColorFix.cpp
    ${SRC_DIR}/types/colorTable.cpp
    ${SRC_DIR}/types/convert.cpp
    ${SRC_DIR}/types/GlyphWidth.cpp
    ${SRC_DIR}/types/sgrStack.cpp
    ${SRC_DIR}/types/utils_linux.cpp
    ${SRC_DIR}/types/viewport.cpp
)
target_link_libraries(types PUBLIC til)

# terminal/parser: VT input/output state machines
add_library(terminal_parser STATIC
    ${SRC_DIR}/terminal/parser/precomp.cpp
    ${SRC_DIR}/terminal/parser/base64.cpp
    ${SRC_DIR}/terminal/parser/InputStateMachineEngine.cpp
    ${SRC_DIR}/terminal/parser/OutputStateMachineEngine.cpp
    ${SRC_DIR}/terminal/parser/stateMachine.cpp
    ${SRC_DIR}/terminal/parser/tracing.cpp
    ${SRC_DIR}/terminal/parser/inputStubs_linux.cpp
)
target_link_libraries(terminal_parser PUBLIC types)

# terminal/adapter: adapters that translate parser dispatch into console ops
add_library(terminal_adapter STATIC
    ${SRC_DIR}/terminal/adapter/precomp.cpp
    ${SRC_DIR}/terminal/adapter/adaptDispatch.cpp
    ${SRC_DIR}/terminal/adapter/adaptDispatchGraphics.cpp
    ${SRC_DIR}/terminal/adapter/FontBuffer.cpp
    ${SRC_DIR}/terminal/adapter/MacroBuffer.cpp
    ${SRC_DIR}/terminal/adapter/PageManager.cpp
    ${SRC_DIR}/terminal/adapter/SixelParser.cpp
    ${SRC_DIR}/terminal/adapter/terminalOutput.cpp
    ${SRC_DIR}/terminal/adapter/shlwapi_linux.cpp
)
target_link_libraries(terminal_adapter PUBLIC types)

# terminal/input: translate keys/mouse into VT sequences
add_library(terminal_input STATIC
    ${SRC_DIR}/terminal/input/precomp.cpp
    ${SRC_DIR}/terminal/input/terminalInput.cpp
    ${SRC_DIR}/terminal/input/mouseInput.cpp
    ${SRC_DIR}/terminal/input/keyboardStubs_linux.cpp
)
target_link_libraries(terminal_input PUBLIC types)

# buffer/out: the console output buffer (Row cells, text buffer, cursor, search)
add_library(buffer STATIC
    ${SRC_DIR}/buffer/out/precomp.cpp
    ${SRC_DIR}/buffer/out/cursor.cpp
    ${SRC_DIR}/buffer/out/ImageSlice.cpp
    ${SRC_DIR}/buffer/out/OutputCell.cpp
    ${SRC_DIR}/buffer/out/OutputCellIterator.cpp
    ${SRC_DIR}/buffer/out/OutputCellRect.cpp
    ${SRC_DIR}/buffer/out/OutputCellView.cpp
    ${SRC_DIR}/buffer/out/Row.cpp
    ${SRC_DIR}/buffer/out/TextColor.cpp
    ${SRC_DIR}/buffer/out/TextAttribute.cpp
    ${SRC_DIR}/buffer/out/textBuffer.cpp
    ${SRC_DIR}/buffer/out/textBufferCellIterator.cpp
    ${SRC_DIR}/buffer/out/textBufferTextIterator.cpp
    ${SRC_DIR}/buffer/out/search.cpp
    ${SRC_DIR}/buffer/out/UTextAdapter.cpp
)
target_link_libraries(buffer PUBLIC types)

# renderer/base: RenderSettings (color tables, attribute->RGB resolution)
add_library(renderer_base STATIC
    ${SRC_DIR}/renderer/base/precomp_linux.cpp
    ${SRC_DIR}/renderer/base/RenderSettings.cpp
)
target_include_directories(renderer_base PUBLIC ${SRC_DIR}/renderer/inc ${SRC_DIR}/renderer/base)
target_link_libraries(renderer_base PUBLIC types)

# ------------------------------------------------------------------
# Frontend: Qt6 grid app that owns a pty + engine instance
# ------------------------------------------------------------------
add_executable(openconsole_linux
    ${SRC_DIR}/frontend/main.cpp
    ${SRC_DIR}/frontend/TerminalWidget.cpp
)
target_include_directories(openconsole_linux PRIVATE ${SRC_DIR}/frontend ${SRC_DIR}/renderer/inc)
target_link_libraries(openconsole_linux PRIVATE til types terminal_parser terminal_adapter terminal_input buffer renderer_base)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
pkg_check_modules(ICU REQUIRED IMPORTED_TARGET icu-uc icu-i18n)
pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)
target_link_libraries(openconsole_linux PRIVATE Qt6::Widgets PkgConfig::ICU PkgConfig::FMT)

# ------------------------------------------------------------------
# test_input: headless check of TerminalInput::HandleKey output
# ------------------------------------------------------------------
add_executable(test_input ${SRC_DIR}/frontend/test_input.cpp)
target_include_directories(test_input PRIVATE ${SRC_DIR}/frontend ${SRC_DIR}/renderer/inc)
target_link_libraries(test_input PRIVATE til types terminal_parser terminal_adapter terminal_input buffer renderer_base PkgConfig::ICU PkgConfig::FMT)

# ------------------------------------------------------------------
# test_copy: verify GetPlainText(CopyRequest) extraction
# ------------------------------------------------------------------
add_executable(test_copy ${SRC_DIR}/frontend/test_copy.cpp)
target_include_directories(test_copy PRIVATE ${SRC_DIR}/frontend ${SRC_DIR}/renderer/inc)
target_link_libraries(test_copy PRIVATE til types terminal_parser terminal_adapter terminal_input buffer renderer_base PkgConfig::ICU PkgConfig::FMT)