#pragma once

// PortManager.h : 端口枚举与进程管理功能的公共接口。

#include <Windows.h>
#include <string>
#include <vector>

// 单条端口记录。
struct PortEntry
{
    std::wstring protocol;      // 协议：TCP 或 UDP
    std::wstring localAddress;  // 本地地址（IPv4 或 IPv6 字符串）
    DWORD        localPort = 0; // 本地端口
    DWORD        pid = 0;       // 所属进程 ID
    std::wstring processName;   // 进程名称
    std::wstring processPath;   // 进程完整路径（无权限访问时为空）
};

// 枚举系统上所有 TCP 侦听端口与 UDP 已绑定端口，并按端口号升序返回。
std::vector<PortEntry> EnumListeningPorts();

// 结束指定进程。
// 返回 true 表示成功；失败时 errMessage 返回面向用户的错误说明。
bool KillProcess(DWORD pid, std::wstring& errMessage);
