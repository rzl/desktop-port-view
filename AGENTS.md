# AGENTS.md

本文件面向 AI 编码助手，介绍本项目的架构、构建方式与开发约定。阅读本文件的读者默认对本项目一无所知。

## 项目概述

**肉丸子桌面端口管理工具** 是一个 Windows 桌面应用程序，用于查看系统侦听的端口及相关进程信息，并支持结束进程。

- 功能：主窗口以列表展示所有 TCP 侦听端口与 UDP 已绑定端口（协议、本地地址、端口、PID、进程名称、进程路径），**表头下方有每列对应的过滤输入框**（不区分大小写子串匹配，即时生效，不影响底层快照）；支持点击列头排序（端口/PID 按数值排序）；**自动刷新默认关闭**，可在「视图→自动刷新」开启（勾选后每 3 秒刷新）；手动刷新（F5 / 「视图→刷新」/ 列表右键菜单，**主界面无刷新按钮**）；双击列表项或右键「结束进程」经确认后结束对应进程（自动尝试提权 `SeDebugPrivilege`，对其他用户的进程需以管理员身份运行）。
- 技术栈：C++ + Win32 API（**不使用** MFC、不使用 .NET），界面使用系统 ListView 通用控件，资源由 `.rc` 脚本定义。
- 目标平台：Windows 10/11（`WindowsTargetPlatformVersion` 为 `10.0`），架构支持 Win32 (x86) 和 x64。
- 界面语言为简体中文（菜单「文件(&F)」「视图(&V)」「帮助(&H)」，关于对话框「版本 1.0，版权所有 (c) 2026」）。
- 远程仓库：`https://github.com/rzl/desktop-port-view.git`（GitHub）。

## 目录结构与关键文件

项目为**单项目、扁平结构**（所有源文件都在仓库根目录，无子目录模块划分）：

| 文件 | 说明 |
| --- | --- |
| `肉丸子桌面端口管理工具.sln` | Visual Studio 解决方案（VS 2022, Format 12.00） |
| `肉丸子桌面端口管理工具.vcxproj` | MSBuild 项目文件，包含全部编译/链接配置 |
| `肉丸子桌面端口管理工具.cpp` | 主源文件：`wWinMain` 入口、窗口类注册、主窗口过程 `WndProc`（含 ListView 界面创建、刷新、排序、右键菜单、结束进程交互）、`About` 对话框过程 |
| `PortManager.h` / `PortManager.cpp` | 端口枚举与进程管理模块：`PortEntry` 结构、`EnumListeningPorts()`（基于 `GetExtendedTcpTable`/`GetExtendedUdpTable`，IPv4+IPv6）、`KillProcess()`（提权 + `TerminateProcess`）；通过 `#pragma comment` 链接 `Iphlpapi.lib`、`Ws2_32.lib`、`Advapi32.lib`（间接） |
| `肉丸子桌面端口管理工具.h` | 仅包含 `#include "resource.h"` |
| `framework.h` | 预编译头式的公共包含文件：定义 `WIN32_LEAN_AND_MEAN`，包含 `<windows.h>` 及 C 运行时头 |
| `targetver.h` | 通过 `SDKDDKVer.h` 指定 Windows SDK 版本 |
| `Resource.h` | 资源 ID 定义（由 Visual C++ 自动生成，手动编辑时注意 `APSTUDIO` 宏区域） |
| `肉丸子桌面端口管理工具.rc` | 资源脚本（**UTF-16 LE 编码**，无法用普通文本工具直接读取，需用支持 UTF-16 的工具，如 PowerShell `[System.IO.File]::ReadAllText($path, [System.Text.Encoding]::Unicode)`）；定义图标、菜单、快捷键、关于对话框、字符串表 |
| `肉丸子桌面端口管理工具.ico` / `small.ico` | 应用程序图标 |
| `肉丸子桌面端口管理工具.vcxproj.filters` | Visual Studio 解决方案资源管理器中的文件分组 |

## 构建与运行

**前置条件**：Windows 上安装 Visual Studio 2022（工具集 **v143**）并勾选「使用 C++ 的桌面开发」工作负荷；Windows 10 SDK（10.0 或更高）。

**使用 Visual Studio IDE**（推荐）：
- 打开 `肉丸子桌面端口管理工具.sln`，选择配置/平台（Debug|x64、Release|x64、Debug|Win32、Release|Win32 均可），按 F5 调试或 Ctrl+Shift+B 生成。

**命令行构建**（在项目根目录执行）：
```powershell
# 先定位 vswhere 找到 MSBuild，例如：
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
# 构建 Release x64
& $msbuild "肉丸子桌面端口管理工具.sln" /p:Configuration=Release /p:Platform=x64
# 或构建 Win32：
& $msbuild "肉丸子桌面端口管理工具.sln" /p:Configuration=Debug /p:Platform=Win32
```

构建产物输出到 `x64\Debug\`、`x64\Release\`、`Debug\`、`Release\` 等目录（均已列入 `.gitignore`）。子系统为 `Windows`（GUI 程序，无控制台窗口）。

## 测试

- 项目**没有任何自动化测试**（无测试框架、无测试目录）。目前没有 CI/CD 流水线。
- 验证方式：编译通过后运行程序，人工检查——列表是否显示 TCP 侦听/UDP 端口及进程信息；表头下过滤输入框输入关键字即时过滤；点击列头排序；「视图→自动刷新」勾选后 3 秒定时刷新，取消勾选停止；**焦点在过滤框内时按 F5 也能刷新**；「视图→刷新」与列表右键菜单（刷新在结束进程上方）；双击或右键「结束进程」（可先用 `python -m http.server <端口>` 起一个测试进程）；「帮助→关于」对话框。

## 代码风格与约定

- **语言/编码**：源代码注释、UI 字符串均为简体中文；源文件保存为带 BOM 的 UTF-8；`.rc` 文件为 UTF-16 LE。
- **字符集**：Unicode（项目设置 `CharacterSet=Unicode`，入口为 `wWinMain`，统一使用 `WCHAR`/宽字符 API 如 `CreateWindowW`、`RegisterClassExW`）。新增代码请保持宽字符，不要混用 ANSI API。
- **编译设置**：警告等级 Level3，开启 `SDLCheck`（安全检查）和 `ConformanceMode`（标准符合性模式）；Release 开启函数级链接与内联函数优化。不要修改这些默认编译选项，除非确有必要。
- **代码组织**：遵循向导生成骨架的模式——全局变量（`hInst`、`szTitle`、`szWindowClass`）+ 前向声明 + `MyRegisterClass` / `InitInstance` / `WndProc` / `About` 四个函数。新增窗口或对话框时，仿照该模式添加消息处理过程并在 `.rc` / `Resource.h` 中注册资源 ID。
- **资源 ID**：在 `Resource.h` 中定义（注意保留 `APSTUDIO_INVOKED` 宏块中的 `_APS_NEXT_*` 计数宏，供资源编辑器使用）。
- 未使用的参数用 `UNREFERENCED_PARAMETER` 标注。

## 版本控制

- `.gitignore` 采用官方 Visual Studio 模板，忽略 `.vs/`、构建输出目录、`*.user` 等，提交前不要强行添加这些文件。
- Git 历史仅 2 个提交（初始 `.gitattributes`/`.gitignore` + 项目文件），属全新项目。
- 不要执行 `git commit` / `git push` 等写操作，除非用户明确要求。

## 开发本项目时的注意事项

- `PortManager.cpp` 中必须先包含 `<ws2tcpip.h>` 再包含 `<iphlpapi.h>`，否则 IPv6 的 `MIB_TCP6TABLE_OWNER_PID` 等结构（受 `_WS2IPDEF_` 保护）不可见。
- 修改 `Resource.h` 或 `.rc` 后，注意保持两者 ID 一致；`.rc` 必须保持 UTF-16 LE 编码保存，否则资源编译会乱码。用脚本修改 `.rc` 时若发生 PowerShell 语句级异常，脚本可能继续执行并在错误位置写入内容，操作后务必重新读回校验。
- 文件名（源码、项目文件）均为中文，Shell 脚本中引用路径时需注意编码问题，优先使用带引号的绝对路径。
- 新增端口/进程相关功能时，在 `PortManager.cpp` 中扩展并保持 `PortEntry` 结构与列表列（`PortColumn` 枚举）同步。
- `wWinMain` 消息循环中 `TranslateAccelerator` 的第一个参数必须传主窗口句柄（`g_hMainWnd`），不能传 `msg.hwnd`——否则焦点在过滤输入框等子控件上时，F5 触发的 `WM_COMMAND` 会发给子控件而非 `WndProc`，快捷键失效。
