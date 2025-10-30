#pragma once

#include <QObject>
#include <Windows.h>
#include <string>
#include <vector>

class WindowsTrayManager : public QObject
{
    Q_OBJECT
public:
    static WindowsTrayManager& instance();

    bool initialize();
    void shutdown();
    bool minimizeWindowToTray(HWND hwnd);
    void restoreAllWindows();
    bool isInitialized() const { return m_initialized; }
    bool restoreWindow(HWND hwnd);
    void setHotkeyEnabled(bool enabled);

    std::vector<std::pair<HWND, std::wstring>> getHiddenWindows() const;

signals:
    void trayWindowsChanged();

private:
    WindowsTrayManager();
    ~WindowsTrayManager();

    void saveHiddenWindows();
    void restoreHiddenWindows();
    std::wstring getWindowTitle(HWND hwnd) const;

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    struct HiddenWindow {
        HWND hwnd;
    };

    HWND m_mainWindow = nullptr;
    std::vector<HiddenWindow> m_hiddenWindows;
    bool m_initialized = false;
    HANDLE m_saveFile = INVALID_HANDLE_VALUE;
    HANDLE m_mutex = nullptr;

    static WindowsTrayManager* s_instance;

    static constexpr int MAX_WINDOWS = 50;

    bool m_hotkeyEnabled = true;
};