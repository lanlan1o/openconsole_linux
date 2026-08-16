# Crossole (openconsole_linux)

Windows Terminal / OpenConsole 终端引擎的 Linux 移植项目。复用微软 OpenConsole 的
VT 解析与渲染引擎（约 95% 的上游代码），通过一个轻量兼容层屏蔽 Windows API 依赖，
前端用 Qt6 绘制终端网格。

## 特点

- **完整的 VT 引擎**：VT100+ 状态机（`StateMachine`）、`AdaptDispatch` 指令派发、
  Sixel 图形、宏缓冲、页面管理、鼠标上报（SGR/X10）、键盘编码、IME 输入法支持。
- **缓冲区与渲染**：`TextBuffer` / `Row` 单元模型、宽字符（DBCS）处理、调色板
  解析、选区与剪贴板复制粘贴（支持块选择，Alt+拖拽）。
- **兼容层**：`compat/` 提供 `windows.h`、WIL、SAL、intsafe、Shlwapi 等桩头文件，
  使上游 Windows 代码无需改动即可编译；`wistd = std` 别名与 `preinclude.h`
  强制预包含保证各编译单元一致。
- **原生 Linux 前端**：Qt6 Widgets 网格渲染 + `forkpty` 伪终端，支持等宽字体 /
  字号切换、emoji 与 Nerd Font 回退、光标闪烁、窗口标题跟随程序标题。

## 目录结构

```
linux_port/
├── CMakeLists.txt          # CMake 构建脚本（静态库分层 + 前端程序）
├── compat/                 # Windows API 兼容层（桩头文件）
│   ├── preinclude.h        # 强制预包含：STL/fmt/GSL/SAL/WIL/safe_math
│   ├── windows.h           # Windows 头文件桩
│   ├── wil/                # WIL 桩（scope_exit、HR 宏等）
│   ├── sal.h intsafe.h Shlwapi.h ...
│   └── gsl/                # 微软 GSL（与 temp_gsl 等价）
├── temp_gsl/               # GSL 头文件副本
├── src/
│   ├── til/                # 工具库（彩色、坐标、UTF 转换等，多为主头文件）
│   ├── inc/                # 公共头（til/、测试等）
│   ├── types/              # Viewport、调色板、字形宽度、颜色转换
│   ├── terminal/
│   │   ├── parser/         # VT 输入/输出状态机
│   │   ├── adapter/        # AdaptDispatch、SixelParser、FontBuffer、PageManager
│   │   └── input/          # 按键/鼠标 -> VT 序列
│   ├── buffer/out/         # 屏幕缓冲区（TextBuffer、Row、光标、搜索）
│   ├── renderer/base/      # RenderSettings 颜色表
│   ├── interactivity/      # （预留）
│   └── frontend/           # Qt6 前端：TerminalWidget、TerminalApi、main
├── build/                  # CMake/Ninja 构建输出
└── oss/                    # 第三方头文件依赖（Chromium safe_math 等，许可见下）
```

## 构建

### 依赖

- CMake ≥ 3.20，Ninja，支持 C++20 的编译器（GCC/Clang）
- Qt6（Widgets 模块）
- ICU（`icu-uc`、`icu-i18n`）
- fmt
- 编译期依赖 `oss/`（Chromium safe_math 等），已随本仓库置于项目目录下

Arch 系（含 CachyOS）：

```sh
sudo pacman -S cmake ninja gcc qt6-base icu fmt pkgconf
```

Debian/Ubuntu：

```sh
sudo apt install cmake ninja-build g++ qt6-base-dev libicu-dev libfmt-dev pkg-config
```

### 配置与编译

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

产物：

| 目标 | 说明 |
|------|------|
| `build/openconsole_linux` | 终端主程序（Qt6 网格前端） |
| `build/test_input` | 无头测试：TerminalInput::HandleKey 按键编码 |
| `build/test_copy` | 无头测试：GetPlainText 剪贴板文本提取 |

> 注意：`CMakeLists.txt` 通过 `oss/` 引用第三方头文件（Chromium safe_math、
> interval_tree）。若移动项目，请确保 `oss/` 仍在项目根目录下。

### 运行

```sh
./build/openconsole_linux
```

## 测试

```sh
./build/test_input
./build/test_copy
```

## 开发者声明

本项目的代码由 AI 生成，使用了以下模型：

- GLM5.2
- GLM5.1
- DeepseekV4Flash/Pro
- Hy3
- Mimov2.5

## 许可

本仓库移植自微软 Windows Terminal / OpenConsole
（[MIT License](https://github.com/microsoft/terminal/blob/main/LICENSE)），
Copyright (c) Microsoft Corporation。新增的 Linux 适配代码同样以 MIT 协议发布。

### 第三方依赖（`oss/`）

`oss/` 随本仓库分发，镜像了以下开源组件
（每个组件目录内含 `cgmanifest.json` 记录来源 commit；部分组件自带 LICENSE 文件）：

| 组件 | 来源 | 许可 |
|------|------|------|
| `chromium/` | [chromium/base](https://github.com/chromium/chromium)（safe_math 等） | BSD-3-Clause（见 `chromium/LICENSE`） |
| `interval_tree/` | [ekg/intervaltree](https://github.com/ekg/intervaltree) | MIT（目录内未附带 LICENSE 文件） |
| `pcg/` | [imneme/pcg-cpp](https://github.com/imneme/pcg-cpp) | Apache-2.0 / MIT 双许可 |
| `stb/` | [nothings/stb](https://github.com/nothings/stb) | MIT / Unlicense 双许可 |
| `wyhash/` | [wangyi-fudan/wyhash](https://github.com/wangyi-fudan/wyhash) | CC0 公共域（见 `wyhash/LICENSE`） |
| `xorg_apps_rgb/` | Xorg 应用集 `rgb.txt` | X11（目录内未附带 LICENSE 文件） |

> 许可详情以各组件目录内的原始许可文件为准；缺失 LICENSE 的组件请参考其上游
> 仓库声明。
