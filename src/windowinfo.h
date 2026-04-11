#pragma once
#include <QIcon>
#include <QList>
#include <QPair>
#include <QString>
#include <windows.h>

struct WindowInfo {
    QString title;
    QString originalTitle;
    QString processName;
    QString processPath;
    QString className;
    DWORD processId;
    HWND hwnd;
    bool isHidden;
    bool isVisible;
    QIcon icon;
};

namespace WindowInfoUtils {
    WindowInfo getWindowInfo(HWND hwnd, bool filterInvisibleChars = true);
    QList<QPair<HWND, WindowInfo>> getAllWindowsInfo();
    QIcon getWindowIcon(HWND hwnd);
    bool isWindowOnTop(HWND hwnd);
    void setWindowOnTop(HWND hwnd, bool onTop);
    void flashWindowInTaskbar(HWND hwnd);
}