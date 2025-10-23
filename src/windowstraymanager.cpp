#include "windowstraymanager.h"
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <psapi.h>

// 静态成员初始化
WindowsTrayManager* WindowsTrayManager::s_instance = nullptr;

WindowsTrayManager& WindowsTrayManager::instance()
{
    if (!s_instance) {
        s_instance = new WindowsTrayManager();
    }
    return *s_instance;
}

WindowsTrayManager::WindowsTrayManager()
    : m_mainWindow(nullptr)
    , m_trayMenu(nullptr)
    , m_initialized(false)
    , m_saveFile(INVALID_HANDLE_VALUE)
    , m_mutex(nullptr)
{
}

WindowsTrayManager::~WindowsTrayManager()
{
    shutdown();
}

bool WindowsTrayManager::initialize()
{
    if (m_initialized) {
        return true;
    }

    // 创建互斥锁防止多实例
    m_mutex = CreateMutex(NULL, TRUE, L"Traymond_MyApplication_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return false;
    }

    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"WindowsTrayManagerClass";

    if (!RegisterClass(&wc)) {
        return false;
    }

    // 创建消息窗口
    m_mainWindow = CreateWindow(
        wc.lpszClassName,
        L"Windows Tray Manager",
        0,  // 不需要样式
        0, 0, 0, 0,
        HWND_MESSAGE,
        NULL,
        GetModuleHandle(NULL),
        this
    );

    if (!m_mainWindow) {
        return false;
    }

    // 注册热键 Win+Shift+Z
    if (!RegisterHotKey(m_mainWindow, 1, MOD_WIN | MOD_SHIFT | MOD_NOREPEAT, 0x5A)) { // 0x5A = 'Z'
        // 热键注册失败不是致命错误，继续初始化
    }

    // 创建托盘菜单
    createTrayMenu();

    // 恢复之前隐藏的窗口
    restoreHiddenWindows();

    m_initialized = true;
    return true;
}

void WindowsTrayManager::shutdown()
{
    if (!m_initialized) {
        return;
    }

    // 恢复所有隐藏的窗口
    restoreAllWindows();

    // 注销热键
    if (m_mainWindow) {
        UnregisterHotKey(m_mainWindow, 1);
    }

    // 清理资源
    if (m_trayMenu) {
        DestroyMenu(m_trayMenu);
    }

    if (m_mainWindow) {
        DestroyWindow(m_mainWindow);
    }

    if (m_mutex) {
        ReleaseMutex(m_mutex);
        CloseHandle(m_mutex);
    }

    if (m_saveFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m_saveFile);
    }

    m_initialized = false;
}

void WindowsTrayManager::createTrayMenu()
{
    m_trayMenu = CreatePopupMenu();

    MENUITEMINFOW menuItem = {};
    menuItem.cbSize = sizeof(MENUITEMINFOW);

    // 恢复所有窗口菜单项
    menuItem.fMask = MIIM_STRING | MIIM_ID;
    menuItem.wID = RESTORE_ALL_ID;

    wchar_t restoreText[] = L"Restore All Windows";
    menuItem.dwTypeData = restoreText;
    menuItem.cch = wcslen(restoreText);
    InsertMenuItem(m_trayMenu, 0, TRUE, &menuItem);

    // 分隔符
    menuItem.fMask = MIIM_TYPE;
    menuItem.fType = MFT_SEPARATOR;
    InsertMenuItem(m_trayMenu, 1, TRUE, &menuItem);

    // 退出菜单项
    menuItem.fMask = MIIM_STRING | MIIM_ID;
    menuItem.fType = MFT_STRING;
    menuItem.wID = EXIT_ID;

    wchar_t exitText[] = L"Exit";
    menuItem.dwTypeData = exitText;
    menuItem.cch = wcslen(exitText);
    InsertMenuItem(m_trayMenu, 2, TRUE, &menuItem);
}

std::vector<std::pair<HWND, std::wstring>> WindowsTrayManager::getHiddenWindows() const
{
    std::vector<std::pair<HWND, std::wstring>> result;
    for (const auto& hiddenWindow : m_hiddenWindows) {
        if (hiddenWindow.hwnd && IsWindow(hiddenWindow.hwnd)) {
            std::wstring title = getWindowTitle(hiddenWindow.hwnd);
            result.emplace_back(hiddenWindow.hwnd, title);
        }
    }
    return result;
}

std::wstring WindowsTrayManager::getWindowTitle(HWND hwnd) const
{
    wchar_t title[256];
    if (GetWindowText(hwnd, title, 256) > 0) {
        return std::wstring(title);
    }
    return L"Unknown Window";
}

bool WindowsTrayManager::minimizeWindowToTray(HWND hwnd)
{
    if (!hwnd || m_hiddenWindows.size() >= MAX_WINDOWS) {
        return false;
    }

    // 检查是否是受保护的窗口
    wchar_t className[256];
    if (!GetClassName(hwnd, className, 256)) {
        return false;
    }

    // 禁止隐藏系统关键窗口
    const wchar_t* restrictedWindows[] = {
        L"WorkerW",
        L"Shell_TrayWnd",
        L"Progman",
        L"WindowsTrayManagerClass"  // 我们自己的窗口类
    };

    for (const wchar_t* restricted : restrictedWindows) {
        if (wcscmp(className, restricted) == 0) {
            return false;
        }
    }

    // 获取窗口图标
    HICON icon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0);
    if (!icon) {
        icon = (HICON)GetClassLongPtr(hwnd, GCLP_HICONSM);
    }
    if (!icon) {
        icon = LoadIcon(NULL, IDI_APPLICATION);
    }

    // 创建托盘图标
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = m_mainWindow;
    nid.uID = 1000 + (UINT)m_hiddenWindows.size() + 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = icon;

    wchar_t windowTitle[128];
    GetWindowText(hwnd, windowTitle, 128);
    wcscpy_s(nid.szTip, windowTitle);

    bool success = Shell_NotifyIcon(NIM_ADD, &nid);
    if (!success) {
        return false;
    }

    // 设置版本
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIcon(NIM_SETVERSION, &nid);

    // 保存隐藏窗口信息
    HiddenWindow hiddenWindow;
    hiddenWindow.iconData = nid;
    hiddenWindow.hwnd = hwnd;
    m_hiddenWindows.push_back(hiddenWindow);

    // 隐藏窗口
    ShowWindow(hwnd, SW_HIDE);

    // 保存状态
    saveHiddenWindows();

    return true;
}

void WindowsTrayManager::restoreAllWindows()
{
    for (auto& hiddenWindow : m_hiddenWindows) {
        ShowWindow(hiddenWindow.hwnd, SW_SHOW);
        SetForegroundWindow(hiddenWindow.hwnd);
        Shell_NotifyIcon(NIM_DELETE, &hiddenWindow.iconData);
    }
    m_hiddenWindows.clear();

    // 清理保存文件
    DeleteFile(L"traymond_save.dat");
}

void WindowsTrayManager::showWindowFromTray(UINT iconId)
{
    auto it = std::find_if(m_hiddenWindows.begin(), m_hiddenWindows.end(),
        [iconId](const HiddenWindow& hw) {
            return hw.iconData.uID == iconId;
        });

    if (it != m_hiddenWindows.end()) {
        ShowWindow(it->hwnd, SW_SHOW);
        SetForegroundWindow(it->hwnd);
        Shell_NotifyIcon(NIM_DELETE, &it->iconData);
        m_hiddenWindows.erase(it);

        saveHiddenWindows();
    }
}

void WindowsTrayManager::saveHiddenWindows()
{
    std::wofstream file("traymond_save.dat");
    if (file.is_open()) {
        for (const auto& hiddenWindow : m_hiddenWindows) {
            file << reinterpret_cast<uintptr_t>(hiddenWindow.hwnd) << std::endl;
        }
        file.close();
    }
}

void WindowsTrayManager::restoreHiddenWindows()
{
    std::wifstream file("traymond_save.dat");
    if (!file.is_open()) {
        return;
    }

    std::wstring line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            try {
                uintptr_t hwndValue = std::stoull(line);
                HWND hwnd = reinterpret_cast<HWND>(hwndValue);

                // 验证窗口是否仍然存在
                if (IsWindow(hwnd)) {
                    minimizeWindowToTray(hwnd);
                }
            }
            catch (const std::exception&) {
                // 忽略转换错误
            }
        }
    }

    file.close();
}

LRESULT CALLBACK WindowsTrayManager::windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WindowsTrayManager* manager = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        manager = reinterpret_cast<WindowsTrayManager*>(createStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(manager));
    }
    else {
        manager = reinterpret_cast<WindowsTrayManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!manager) {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
    case WM_HOTKEY:
        if (wParam == 1) { // 我们的热键ID
            HWND foregroundWindow = GetForegroundWindow();
            if (foregroundWindow && foregroundWindow != hwnd) {
                manager->minimizeWindowToTray(foregroundWindow);
            }
        }
        break;

    case WM_TRAYICON:// 托盘图标双击恢复功能
        if (lParam == WM_LBUTTONDBLCLK) {
            manager->showWindowFromTray(static_cast<UINT>(wParam));
        }
        else if (lParam == WM_RBUTTONUP) {
            // 显示上下文菜单
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd); // 确保菜单能正确消失
            TrackPopupMenu(manager->m_trayMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            PostMessage(hwnd, WM_NULL, 0, 0);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case RESTORE_ALL_ID:
            manager->restoreAllWindows();
            break;
        case EXIT_ID:
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

bool WindowsTrayManager::restoreWindow(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }

    // 查找对应的托盘图标
    auto it = std::find_if(m_hiddenWindows.begin(), m_hiddenWindows.end(),
        [hwnd](const HiddenWindow& hiddenWindow) {
            return hiddenWindow.hwnd == hwnd;
        });

    if (it == m_hiddenWindows.end()) {
        return false;
    }

    // 恢复窗口显示
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    // 移除托盘图标
    Shell_NotifyIcon(NIM_DELETE, &it->iconData);

    // 从列表中移除
    m_hiddenWindows.erase(it);

    // 更新保存文件
    saveHiddenWindows();

    return true;
}