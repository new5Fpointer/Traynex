#pragma once

#include <Windows.h>
#include <string>
#include <vector>

class WindowsTrayManager
{
public:
    static WindowsTrayManager& instance();

    bool initialize();
    void shutdown();
    bool minimizeWindowToTray(HWND hwnd);
    void restoreAllWindows();
    bool isInitialized() const { return m_initialized; }
    bool restoreWindow(HWND hwnd);
    void setHotkeyEnabled(bool enabled);

    // 获取隐藏窗口信息
    std::vector<std::pair<HWND, std::wstring>> getHiddenWindows() const;

private:
    WindowsTrayManager();
    ~WindowsTrayManager();

    void createTrayMenu();
    void saveHiddenWindows();
    void restoreHiddenWindows();
    void showWindowFromTray(UINT iconId);
    std::wstring getWindowTitle(HWND hwnd) const;

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    struct HiddenWindow {
        NOTIFYICONDATA iconData;
        HWND hwnd;
    };

    HWND m_mainWindow = nullptr;
    HMENU m_trayMenu = nullptr;
    std::vector<HiddenWindow> m_hiddenWindows;
    bool m_initialized = false;
    HANDLE m_saveFile = INVALID_HANDLE_VALUE;
    HANDLE m_mutex = nullptr;

    static WindowsTrayManager* s_instance;

    static constexpr UINT WM_TRAYICON = WM_USER + 101;
    static constexpr int EXIT_ID = 1001;
    static constexpr int RESTORE_ALL_ID = 1002;
    static constexpr int MAX_WINDOWS = 50;

    bool m_hotkeyEnabled = true;
};