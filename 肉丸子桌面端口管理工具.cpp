// 肉丸子桌面端口管理工具.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "肉丸子桌面端口管理工具.h"
#include "PortManager.h"

#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

// 通用控件库（InitCommonControlsEx 等）
#pragma comment(lib, "Comctl32.lib")

#define MAX_LOADSTRING 100

// 列索引定义
enum PortColumn
{
    ColProtocol = 0,
    ColAddress,
    ColPort,
    ColPid,
    ColProcessName,
    ColProcessPath,
    ColCount
};

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名

HWND g_hList = nullptr;                         // 端口列表控件
HWND g_hMainWnd = nullptr;                      // 主窗口句柄
BOOL g_autoRefresh = FALSE;                     // 是否自动刷新（默认关闭）
HWND g_hFilter[ColCount] = {};                  // 每列的过滤输入框
std::vector<PortEntry> g_allEntries;            // 最近一次枚举的完整端口记录

// 布局常量：过滤行、列表
const int kMargin = 8;
const int kFilterHeight = 22;
const int kFilterTop = kMargin;
const int kListTop = kFilterTop + kFilterHeight + 4;

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// 端口列表相关的辅助函数
HWND    CreatePortList(HWND hWndParent);
void    CreateFilterEdits(HWND hWndParent);
void    PositionFilters(HWND hWnd);
void    ResizeChildren(HWND hWnd);
void    RebuildList(HWND hWnd);
void    FillList();
BOOL    FilterMatches(const PortEntry& entry);
void    ToggleAutoRefresh(HWND hWnd);
BOOL    KillSelectedProcess(HWND hWnd);
BOOL    GetSelectedEntry(HWND hList, PortEntry& entry);
int CALLBACK ComparePortItem(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 初始化通用控件（ListView 需要）
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MY, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MY));

    MSG msg;

    // 主消息循环:
    // 注意：TranslateAccelerator 的第一个参数必须是主窗口，
    // 否则当焦点在过滤输入框等子控件上时，F5 产生的 WM_COMMAND 会发给子控件而非 WndProc。
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(g_hMainWnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MY));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_MY);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 960, 640, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   g_hMainWnd = hWnd;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND     - 处理应用程序菜单与控件命令
//  WM_CREATE      - 创建“刷新”按钮与端口列表控件
//  WM_SIZE        - 调整子控件布局
//  WM_TIMER       - 定时自动刷新端口列表
//  WM_NOTIFY      - 列头排序、双击列表项
//  WM_CONTEXTMENU - 列表右键菜单
//  WM_DESTROY     - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        g_hList = CreatePortList(hWnd);
        if (g_hList == nullptr)
        {
            return -1;
        }

        CreateFilterEdits(hWnd);
        ResizeChildren(hWnd);

        // 自动刷新默认关闭，同步菜单复选状态（不启动定时器）。
        CheckMenuItem(GetMenu(hWnd), IDM_AUTO_REFRESH, MF_BYCOMMAND | MF_UNCHECKED);

        RebuildList(hWnd);
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 过滤输入框内容变化：仅按当前快照重新填充列表，不重新枚举。
            if (HIWORD(wParam) == EN_CHANGE)
            {
                for (int i = 0; i < ColCount; ++i)
                {
                    if ((HWND)lParam == g_hFilter[i])
                    {
                        FillList();
                        return 0;
                    }
                }
            }
            // 分析菜单选择:
            switch (wmId)
            {
            case IDM_REFRESH:
                RebuildList(hWnd);
                break;
            case IDM_AUTO_REFRESH:
                ToggleAutoRefresh(hWnd);
                break;
            case IDM_KILL_PROCESS:
                KillSelectedProcess(hWnd);
                break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_SIZE:
        ResizeChildren(hWnd);
        break;
    case WM_TIMER:
        if (wParam == ID_TIMER_REFRESH && g_autoRefresh)
        {
            RebuildList(hWnd);
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
            if (pnmh->hwndFrom == g_hList)
            {
                switch (pnmh->code)
                {
                case LVN_COLUMNCLICK:
                    {
                        LPNMLISTVIEW pnmlv = reinterpret_cast<LPNMLISTVIEW>(lParam);
                        static int s_sortColumn = -1;
                        static BOOL s_ascending = TRUE;
                        if (s_sortColumn == pnmlv->iSubItem)
                        {
                            s_ascending = !s_ascending;
                        }
                        else
                        {
                            s_sortColumn = pnmlv->iSubItem;
                            s_ascending = TRUE;
                        }
                        LPARAM sortParam = (s_sortColumn << 1) | (s_ascending ? 1 : 0);
                        ListView_SortItems(g_hList, ComparePortItem, sortParam);
                    }
                    break;
                case NM_DBLCLK:
                    KillSelectedProcess(hWnd);
                    break;
                default:
                    break;
                }
            }
            else if (pnmh->code == HDN_ENDTRACK)
            {
                // 列宽调整结束后，重新对齐过滤输入框。
                PositionFilters(hWnd);
            }
        }
        break;
    case WM_CONTEXTMENU:
        if ((HWND)wParam == g_hList)
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (pt.x == -1 && pt.y == -1)
            {
                // 键盘唤出的菜单：定位到选中项
                int item = ListView_GetNextItem(g_hList, -1, LVNI_SELECTED);
                if (item >= 0)
                {
                    RECT rc = {};
                    ListView_GetItemRect(g_hList, item, &rc, LVIR_BOUNDS);
                    pt.x = rc.left + 20;
                    pt.y = rc.bottom;
                    ClientToScreen(g_hList, &pt);
                }
                else
                {
                    pt.x = 0;
                    pt.y = 0;
                }
            }

            HMENU hMenu = CreatePopupMenu();
            if (hMenu)
            {
                AppendMenuW(hMenu, MF_STRING, IDM_REFRESH, L"刷新(&R)");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, IDM_KILL_PROCESS, L"结束进程(&K)");
                TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
                DestroyMenu(hMenu);
            }
        }
        break;
    case WM_DESTROY:
        KillTimer(hWnd, ID_TIMER_REFRESH);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 创建端口列表控件并添加各列。
HWND CreatePortList(HWND hWndParent)
{
    HWND hList = CreateWindowW(WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, hWndParent, (HMENU)(INT_PTR)ID_LIST_PORTS, hInst, nullptr);
    if (hList == nullptr)
    {
        return nullptr;
    }

    ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    const struct { const WCHAR* text; int width; } columns[ColCount] = {
        { L"协议",       60  },
        { L"本地地址",   170 },
        { L"本地端口",   80  },
        { L"PID",        80  },
        { L"进程名称",   160 },
        { L"进程路径",   380 },
    };

    for (int i = 0; i < ColCount; ++i)
    {
        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.iSubItem = i;
        col.pszText = const_cast<WCHAR*>(columns[i].text);
        col.cx = columns[i].width;
        ListView_InsertColumn(hList, i, &col);
    }

    return hList;
}

// 获取当前选中的端口记录。
BOOL GetSelectedEntry(HWND hList, PortEntry& entry)
{
    int item = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
    if (item < 0)
    {
        return FALSE;
    }

    auto getText = [hList, item](int subItem, std::wstring& out) {
        WCHAR buf[512] = {};
        ListView_GetItemText(hList, item, subItem, buf, _countof(buf));
        out = buf;
    };

    getText(ColProtocol, entry.protocol);
    getText(ColAddress, entry.localAddress);
    entry.localPort = 0;
    entry.pid = 0;

    WCHAR buf[32] = {};
    ListView_GetItemText(hList, item, ColPort, buf, _countof(buf));
    entry.localPort = static_cast<DWORD>(_wtoi(buf));
    ListView_GetItemText(hList, item, ColPid, buf, _countof(buf));
    entry.pid = static_cast<DWORD>(_wtoi(buf));

    getText(ColProcessName, entry.processName);
    getText(ColProcessPath, entry.processPath);
    return TRUE;
}

// 为每一列创建过滤输入框（位于表头下方，与列对齐）。
void CreateFilterEdits(HWND hWndParent)
{
    for (int i = 0; i < ColCount; ++i)
    {
        g_hFilter[i] = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 0, 0, hWndParent, (HMENU)(INT_PTR)(ID_FILTER_BASE + i), hInst, nullptr);
        if (g_hFilter[i])
        {
            // 灰色提示文字，说明该输入框的用途。
            SendMessageW(g_hFilter[i], EM_SETCUEBANNER, FALSE,
                reinterpret_cast<LPARAM>(L"输入以过滤…"));
        }
    }
}

// 按各列当前的位置与宽度重新排列过滤输入框。
void PositionFilters(HWND hWnd)
{
    if (g_hList == nullptr)
    {
        return;
    }

    HWND hHeader = ListView_GetHeader(g_hList);
    RECT rcClient = {};
    GetClientRect(hWnd, &rcClient);

    int left = kMargin;
    for (int i = 0; i < ColCount; ++i)
    {
        RECT rcCol = {};
        int width = 100;
        if (hHeader && Header_GetItemRect(hHeader, i, &rcCol))
        {
            // 表头坐标 → 主窗口客户区坐标。
            MapWindowPoints(hHeader, hWnd, reinterpret_cast<POINT*>(&rcCol), 2);
            left = rcCol.left;
            width = rcCol.right - rcCol.left;
        }

        // 最后一列（进程路径）向右延伸填满剩余空间。
        if (i == ColCount - 1)
        {
            width = rcClient.right - kMargin - left;
            if (width < 60)
            {
                width = 60;
            }
        }

        if (g_hFilter[i])
        {
            SetWindowPos(g_hFilter[i], nullptr, left, kFilterTop,
                width, kFilterHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }
}

// 判断一条记录是否通过所有列的过滤条件（不区分大小写的子串匹配，空条件不过滤）。
BOOL FilterMatches(const PortEntry& entry)
{
    auto matchesOne = [](HWND hEdit, const std::wstring& text) {
        if (hEdit == nullptr)
        {
            return true;
        }
        WCHAR filter[256] = {};
        GetWindowTextW(hEdit, filter, _countof(filter));
        std::wstring f(filter);
        // 去除首尾空白。
        const size_t begin = f.find_first_not_of(L" \t");
        if (begin == std::wstring::npos)
        {
            return true;
        }
        const size_t end = f.find_last_not_of(L" \t");
        f = f.substr(begin, end - begin + 1);

        std::wstring hay(text);
        std::transform(f.begin(), f.end(), f.begin(), towlower);
        std::transform(hay.begin(), hay.end(), hay.begin(), towlower);
        return hay.find(f) != std::wstring::npos;
    };

    if (!matchesOne(g_hFilter[ColProtocol], entry.protocol)) return FALSE;
    if (!matchesOne(g_hFilter[ColAddress], entry.localAddress)) return FALSE;
    if (!matchesOne(g_hFilter[ColPort], std::to_wstring(entry.localPort))) return FALSE;
    if (!matchesOne(g_hFilter[ColPid], std::to_wstring(entry.pid))) return FALSE;
    if (!matchesOne(g_hFilter[ColProcessName], entry.processName)) return FALSE;
    if (!matchesOne(g_hFilter[ColProcessPath], entry.processPath)) return FALSE;
    return TRUE;
}

// 按当前过滤条件把快照中的记录填充到列表（不重新枚举）。
void FillList()
{
    if (g_hList == nullptr)
    {
        return;
    }

    ListView_DeleteAllItems(g_hList);

    for (const PortEntry& e : g_allEntries)
    {
        if (!FilterMatches(e))
        {
            continue;
        }

        WCHAR portText[16] = {};
        swprintf_s(portText, L"%lu", e.localPort);
        WCHAR pidText[16] = {};
        swprintf_s(pidText, L"%lu", e.pid);

        LVITEMW item = {};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = INT_MAX;
        item.iSubItem = ColProtocol;
        item.pszText = const_cast<WCHAR*>(e.protocol.c_str());
        const int index = ListView_InsertItem(g_hList, &item);
        item.lParam = index;
        ListView_SetItem(g_hList, &item);

        ListView_SetItemText(g_hList, index, ColAddress, const_cast<WCHAR*>(e.localAddress.c_str()));
        ListView_SetItemText(g_hList, index, ColPort, portText);
        ListView_SetItemText(g_hList, index, ColPid, pidText);
        ListView_SetItemText(g_hList, index, ColProcessName, const_cast<WCHAR*>(e.processName.c_str()));
        ListView_SetItemText(g_hList, index, ColProcessPath, const_cast<WCHAR*>(e.processPath.c_str()));
    }
}

// 重新枚举端口，保存快照后按当前过滤条件填充列表。
void RebuildList(HWND hWnd)
{
    if (g_hList == nullptr)
    {
        return;
    }

    EnableWindow(g_hList, FALSE);
    HCURSOR hOldCursor = SetCursor(LoadCursor(nullptr, IDC_WAIT));

    g_allEntries = EnumListeningPorts();
    FillList();

    SetCursor(hOldCursor);
    EnableWindow(g_hList, TRUE);
}

// 调整过滤输入框与列表控件的布局，使其填满客户区。
void ResizeChildren(HWND hWnd)
{
    RECT rc = {};
    GetClientRect(hWnd, &rc);

    if (g_hList)
    {
        SetWindowPos(g_hList, nullptr, kMargin, kListTop,
            rc.right - kMargin * 2, rc.bottom - kListTop - kMargin, SWP_NOZORDER);
        PositionFilters(hWnd);
    }
}

// 开关自动刷新，并同步菜单复选状态。
void ToggleAutoRefresh(HWND hWnd)
{
    g_autoRefresh = !g_autoRefresh;
    if (g_autoRefresh)
    {
        SetTimer(hWnd, ID_TIMER_REFRESH, 3000, nullptr);
    }
    else
    {
        KillTimer(hWnd, ID_TIMER_REFRESH);
    }

    HMENU hMenu = GetMenu(hWnd);
    if (hMenu)
    {
        CheckMenuItem(hMenu, IDM_AUTO_REFRESH,
            MF_BYCOMMAND | (g_autoRefresh ? MF_CHECKED : MF_UNCHECKED));
    }
}

// 结束列表中选中的进程：确认后执行，并立即刷新列表。
BOOL KillSelectedProcess(HWND hWnd)
{
    PortEntry entry;
    if (!GetSelectedEntry(g_hList, entry))
    {
        MessageBoxW(hWnd, L"请先在列表中选择一条端口记录。", L"结束进程", MB_OK | MB_ICONINFORMATION);
        return FALSE;
    }

    WCHAR msg[1024] = {};
    swprintf_s(msg,
        L"确定要结束以下进程吗？\n\n进程名称：%s\n进程 ID：%lu\n可执行文件路径：%s\n\n结束进程可能导致未保存的数据丢失，且需要相应权限（其他用户的进程需以管理员身份运行）。",
        entry.processName.c_str(), entry.pid,
        entry.processPath.empty() ? L"（无法访问）" : entry.processPath.c_str());

    if (MessageBoxW(hWnd, msg, L"结束进程", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
    {
        return FALSE;
    }

    std::wstring err;
    if (KillProcess(entry.pid, err))
    {
        MessageBoxW(hWnd, L"进程已结束。", L"结束进程", MB_OK | MB_ICONINFORMATION);
        RebuildList(hWnd);
        return TRUE;
    }

    MessageBoxW(hWnd, err.c_str(), L"结束进程", MB_OK | MB_ICONERROR);
    return FALSE;
}

// ListView 排序比较回调。按列文本比较；“本地端口”和“PID”列按数值比较。
int CALLBACK ComparePortItem(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    const int column = static_cast<int>(lParamSort >> 1);
    const BOOL ascending = (lParamSort & 1) != 0;

    // lParam1/lParam2 是插入时写入的行号，可能因上次排序而失效，
    // 需反查当前行号后再读取文本。
    LVFINDINFOW find = {};
    find.flags = LVFI_PARAM;
    find.lParam = lParam1;
    const int row1 = ListView_FindItem(g_hList, -1, &find);
    find.lParam = lParam2;
    const int row2 = ListView_FindItem(g_hList, -1, &find);
    if (row1 < 0 || row2 < 0)
    {
        return 0;
    }

    WCHAR text1[512] = {};
    WCHAR text2[512] = {};
    ListView_GetItemText(g_hList, row1, column, text1, _countof(text1));
    ListView_GetItemText(g_hList, row2, column, text2, _countof(text2));

    int result = 0;
    if (column == ColPort || column == ColPid)
    {
        const long n1 = wcstol(text1, nullptr, 10);
        const long n2 = wcstol(text2, nullptr, 10);
        result = (n1 < n2) ? -1 : (n1 > n2) ? 1 : 0;
    }
    else
    {
        result = CompareStringOrdinal(text1, -1, text2, -1, TRUE) - CSTR_EQUAL;
    }

    return ascending ? result : -result;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
