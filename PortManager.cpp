// PortManager.cpp : 端口枚举与进程管理功能的实现。

#include "framework.h"
#include "PortManager.h"

#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <algorithm>

// 链接 IP 帮助函数库与 Winsock（InetNtop 等）。
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace
{

// 从完整路径中解析进程名称（最后一个反斜杠之后、去掉 .exe 后缀）。
std::wstring ExtractProcessName(const std::wstring& path)
{
    size_t pos = path.find_last_of(L'\\');
    std::wstring name = (pos == std::wstring::npos) ? path : path.substr(pos + 1);
    const std::wstring kExeSuffix = L".exe";
    if (name.size() > kExeSuffix.size() &&
        _wcsicmp(name.substr(name.size() - kExeSuffix.size()).c_str(), kExeSuffix.c_str()) == 0)
    {
        name.resize(name.size() - kExeSuffix.size());
    }
    return name;
}

// 根据进程 ID 查询进程路径与名称。查询失败（系统进程/权限不足）时给出占位文本。
void FillProcessInfo(DWORD pid, PortEntry& entry)
{
    entry.pid = pid;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess == nullptr)
    {
        entry.processName = (pid == 0 || pid == 4) ? L"系统" : L"无法访问";
        return;
    }

    WCHAR path[MAX_PATH] = {};
    DWORD size = _countof(path);
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size) && size > 0)
    {
        entry.processPath.assign(path, size);
        entry.processName = ExtractProcessName(entry.processPath);
    }
    else
    {
        entry.processName = (pid == 0 || pid == 4) ? L"系统" : L"无法访问";
    }

    CloseHandle(hProcess);
}

// 将 IPv4/IPv6 地址格式化为字符串。
std::wstring FormatAddress(const void* addr, bool isIpv6)
{
    WCHAR buf[INET6_ADDRSTRLEN] = {};
    const WCHAR* result = InetNtopW(isIpv6 ? AF_INET6 : AF_INET, const_cast<void*>(addr), buf, _countof(buf));
    return result ? std::wstring(buf) : std::wstring();
}

// 枚举 TCP 侦听端口（IPv4 与 IPv6）。
void EnumTcp(std::vector<PortEntry>& entries)
{
    // IPv4
    {
        ULONG size = 0;
        // 先查询所需缓冲区大小。
        if (GetExtendedTcpTable(nullptr, &size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) == ERROR_INSUFFICIENT_BUFFER)
        {
            std::vector<BYTE> buffer(size);
            PMIB_TCPTABLE_OWNER_PID table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
            if (GetExtendedTcpTable(table, &size, TRUE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) == NO_ERROR)
            {
                for (DWORD r = 0; r < table->dwNumEntries; ++r)
                {
                    const MIB_TCPROW_OWNER_PID& row = table->table[r];
                    PortEntry entry;
                    entry.protocol = L"TCP";
                    entry.localAddress = FormatAddress(&row.dwLocalAddr, false);
                    entry.localPort = ntohs(static_cast<u_short>(row.dwLocalPort));
                    FillProcessInfo(row.dwOwningPid, entry);
                    entries.push_back(std::move(entry));
                }
            }
        }
    }

    // IPv6
    {
        ULONG size = 0;
        if (GetExtendedTcpTable(nullptr, &size, TRUE, AF_INET6, TCP_TABLE_OWNER_PID_LISTENER, 0) == ERROR_INSUFFICIENT_BUFFER)
        {
            std::vector<BYTE> buffer(size);
            PMIB_TCP6TABLE_OWNER_PID table = reinterpret_cast<PMIB_TCP6TABLE_OWNER_PID>(buffer.data());
            if (GetExtendedTcpTable(table, &size, TRUE, AF_INET6, TCP_TABLE_OWNER_PID_LISTENER, 0) == NO_ERROR)
            {
                for (DWORD r = 0; r < table->dwNumEntries; ++r)
                {
                    const MIB_TCP6ROW_OWNER_PID& row = table->table[r];
                    PortEntry entry;
                    entry.protocol = L"TCP";
                    entry.localAddress = FormatAddress(row.ucLocalAddr, true);
                    entry.localPort = ntohs(static_cast<u_short>(row.dwLocalPort));
                    FillProcessInfo(row.dwOwningPid, entry);
                    entries.push_back(std::move(entry));
                }
            }
        }
    }
}

// 枚举 UDP 已绑定端口（IPv4 与 IPv6）。
void EnumUdp(std::vector<PortEntry>& entries)
{
    // IPv4
    {
        ULONG size = 0;
        if (GetExtendedUdpTable(nullptr, &size, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0) == ERROR_INSUFFICIENT_BUFFER)
        {
            std::vector<BYTE> buffer(size);
            PMIB_UDPTABLE_OWNER_PID table = reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(buffer.data());
            if (GetExtendedUdpTable(table, &size, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR)
            {
                for (DWORD r = 0; r < table->dwNumEntries; ++r)
                {
                    const MIB_UDPROW_OWNER_PID& row = table->table[r];
                    PortEntry entry;
                    entry.protocol = L"UDP";
                    entry.localAddress = FormatAddress(&row.dwLocalAddr, false);
                    entry.localPort = ntohs(static_cast<u_short>(row.dwLocalPort));
                    FillProcessInfo(row.dwOwningPid, entry);
                    entries.push_back(std::move(entry));
                }
            }
        }
    }

    // IPv6
    {
        ULONG size = 0;
        if (GetExtendedUdpTable(nullptr, &size, TRUE, AF_INET6, UDP_TABLE_OWNER_PID, 0) == ERROR_INSUFFICIENT_BUFFER)
        {
            std::vector<BYTE> buffer(size);
            PMIB_UDP6TABLE_OWNER_PID table = reinterpret_cast<PMIB_UDP6TABLE_OWNER_PID>(buffer.data());
            if (GetExtendedUdpTable(table, &size, TRUE, AF_INET6, UDP_TABLE_OWNER_PID, 0) == NO_ERROR)
            {
                for (DWORD r = 0; r < table->dwNumEntries; ++r)
                {
                    const MIB_UDP6ROW_OWNER_PID& row = table->table[r];
                    PortEntry entry;
                    entry.protocol = L"UDP";
                    entry.localAddress = FormatAddress(row.ucLocalAddr, true);
                    entry.localPort = ntohs(static_cast<u_short>(row.dwLocalPort));
                    FillProcessInfo(row.dwOwningPid, entry);
                    entries.push_back(std::move(entry));
                }
            }
        }
    }
}

// 尝试为当前进程启用调试权限，以便结束其他用户的进程。
void EnableDebugPrivilege()
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        return;
    }

    LUID luid = {};
    if (LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid))
    {
        TOKEN_PRIVILEGES tp = {};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }

    CloseHandle(hToken);
}

} // namespace

std::vector<PortEntry> EnumListeningPorts()
{
    std::vector<PortEntry> entries;
    EnumTcp(entries);
    EnumUdp(entries);

    std::sort(entries.begin(), entries.end(), [](const PortEntry& a, const PortEntry& b) {
        if (a.localPort != b.localPort)
        {
            return a.localPort < b.localPort;
        }
        if (a.protocol != b.protocol)
        {
            return a.protocol < b.protocol;
        }
        return a.localAddress < b.localAddress;
    });

    return entries;
}

bool KillProcess(DWORD pid, std::wstring& errMessage)
{
    errMessage.clear();

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess == nullptr && GetLastError() == ERROR_ACCESS_DENIED)
    {
        // 权限不足时尝试提权后重试一次。
        EnableDebugPrivilege();
        hProcess = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    }

    if (hProcess == nullptr)
    {
        const DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED)
        {
            errMessage = L"访问被拒绝。结束其他用户的进程需要以管理员身份运行本程序。";
        }
        else if (err == ERROR_INVALID_PARAMETER)
        {
            errMessage = L"进程已不存在（可能已自行退出）。";
        }
        else
        {
            WCHAR buf[128] = {};
            swprintf_s(buf, L"无法打开进程，错误码：%lu", err);
            errMessage = buf;
        }
        return false;
    }

    const BOOL ok = TerminateProcess(hProcess, 1);
    if (!ok)
    {
        const DWORD err = GetLastError();
        WCHAR buf[128] = {};
        swprintf_s(buf, L"结束进程失败，错误码：%lu", err);
        errMessage = buf;
    }

    CloseHandle(hProcess);
    return ok == TRUE;
}
