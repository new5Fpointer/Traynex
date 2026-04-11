#include "windowinfo.h"
#include "windowstraymanager.h"
#include <QFileInfo>
#include <QDebug>
#include <QSet>
#include <psapi.h>
#include <shellapi.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")

namespace WindowInfoUtils {

WindowInfo getWindowInfo(HWND hwnd, bool filterInvisibleChars)
{
	WindowInfo info;
	info.hwnd = hwnd;
	
	if (!hwnd || !IsWindow(hwnd)) {
		return info;
	}
	
	// 获取窗口标题
	wchar_t title[256];
	GetWindowText(hwnd, title, 256);
	info.originalTitle = QString::fromWCharArray(title);
	info.title = info.originalTitle;
	
	// 如果需要过滤不可见字符
	if (filterInvisibleChars) {
		info.title.remove(QChar(0x200B));  // 零宽空格
		info.title.remove(QChar(0x200C));  // 零宽非连接符
		info.title.remove(QChar(0x200D));  // 零宽连接符
		info.title.remove(QChar(0xFEFF));  // 零宽无中断空格
	}
	
	// 获取窗口类名
	wchar_t className[256];
	GetClassName(hwnd, className, 256);
	info.className = QString::fromWCharArray(className);
	
	// 获取进程ID
	DWORD processId;
	GetWindowThreadProcessId(hwnd, &processId);
	info.processId = processId;
	
	// 获取进程路径和名称
	info.processPath = "Unknown";
	info.processName = "Unknown";
	
	HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
	if (process) {
		wchar_t processPath[MAX_PATH] = L"";
		if (GetModuleFileNameEx(process, NULL, processPath, MAX_PATH)) {
			info.processPath = QString::fromWCharArray(processPath);
			info.processName = QFileInfo(info.processPath).fileName();
		}
		CloseHandle(process);
	}
	
	// 获取窗口可见性状态
	info.isVisible = IsWindowVisible(hwnd);
	
	// 检查是否为隐藏窗口
	auto hiddenWindows = WindowsTrayManager::instance().getHiddenWindows();
	info.isHidden = false;
	for (const auto& hidden : hiddenWindows) {
		if (hidden.first == hwnd) {
			info.isHidden = true;
			break;
		}
	}
	
	// 获取窗口图标
	info.icon = getWindowIcon(hwnd);
	
	return info;
}

QList<QPair<HWND, WindowInfo>> getAllWindowsInfo()
{
	QList<QPair<HWND, WindowInfo>> windows;
	
	// 获取所有隐藏窗口
	auto hiddenWindows = WindowsTrayManager::instance().getHiddenWindows();
	QSet<HWND> hiddenSet;
	for (const auto& hidden : hiddenWindows) {
		hiddenSet.insert(hidden.first);
	}
	
	EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
		auto windowsList = reinterpret_cast<QList<QPair<HWND, WindowInfo>>*>(lParam);
		DWORD currentProcessId = GetCurrentProcessId();
		
		// 过滤条件
		// 1.窗口有效性
		if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) {
			return TRUE;
		}
		// 2.非自身进程
		DWORD processId;
		GetWindowThreadProcessId(hwnd, &processId);
		if (processId == currentProcessId) {
			return TRUE;
		}
		// 3.工具窗口过滤
		LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
		if (exStyle & WS_EX_TOOLWINDOW) {
			return TRUE;
		}
		// 4.非删除标记
		if (GetProp(hwnd, L"ITaskList_Deleted")) {
			return TRUE;
		}
		// 5.所有者关系和可激活性
		HWND owner = GetWindow(hwnd, GW_OWNER);
		bool hasOwner = (owner != nullptr);
		bool isAppWindow = (exStyle & WS_EX_APPWINDOW);
		
		if (hasOwner && !isAppWindow) {
			return TRUE;
		}
		
		if ((exStyle & WS_EX_NOACTIVATE) && !isAppWindow) {
			return TRUE;
		}
		
		// 6.Application Frame Window 检查
		wchar_t className[256];
		GetClassName(hwnd, className, 256);
		QString windowClass = QString::fromWCharArray(className);
		if (windowClass == "ApplicationFrameWindow" ||
			windowClass == "Windows.UI.Core.CoreWindow" ||
			windowClass == "StartMenuSizingFrame" ||
			windowClass == "Shell_LightDismissOverlay") {
			return TRUE;
		}
		
		// 使用getWindowInfo获取窗口信息
		WindowInfo info = getWindowInfo(hwnd);
		info.isHidden = false; // 会在外部设置
		
		windowsList->append(qMakePair(hwnd, info));
		return TRUE;
	}, reinterpret_cast<LPARAM>(&windows));
	
	// 设置隐藏状态
	for (auto& pair : windows) {
		if (hiddenSet.contains(pair.first)) {
			pair.second.isHidden = true;
		}
	}
	
	return windows;
}

QIcon getWindowIcon(HWND hwnd)
{
	if (!hwnd || !IsWindow(hwnd)) {
		return QIcon();
	}
	
	QIcon windowIcon;
	
	// 尝试获取窗口的小图标
	HICON hIcon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0);
	
	// 尝试获取窗口类的小图标
	if (!hIcon) {
		hIcon = (HICON)GetClassLongPtr(hwnd, GCLP_HICONSM);
	}
	
	// 尝试获取窗口的大图标
	if (!hIcon) {
		hIcon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0);
	}
	
	// 尝试获取窗口类的大图标
	if (!hIcon) {
		hIcon = (HICON)GetClassLongPtr(hwnd, GCLP_HICON);
	}
	
	// 从进程文件获取图标
	if (!hIcon) {
		DWORD processId;
		GetWindowThreadProcessId(hwnd, &processId);
		
		if (processId) {
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
			if (hProcess) {
				wchar_t exePath[MAX_PATH];
				if (GetModuleFileNameEx(hProcess, NULL, exePath, MAX_PATH)) {
					// 提取第一个图标
					hIcon = ExtractIcon(GetModuleHandle(NULL), exePath, 0);
				}
				CloseHandle(hProcess);
			}
		}
	}
	
	// 使用默认应用程序图标
	if (!hIcon) {
		hIcon = LoadIcon(NULL, IDI_APPLICATION);
	}
	
	// 将 HICON 转换为 QIcon
	if (hIcon) {
		QPixmap pixmap = QPixmap::fromImage(QImage::fromHICON(hIcon));
		if (!pixmap.isNull()) {
			windowIcon = QIcon(pixmap);
			
			// 清理提取的图标资源
			if (hIcon != (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0) &&
				hIcon != (HICON)GetClassLongPtr(hwnd, GCLP_HICONSM) &&
				hIcon != (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0) &&
				hIcon != (HICON)GetClassLongPtr(hwnd, GCLP_HICON) &&
				hIcon != LoadIcon(NULL, IDI_APPLICATION)) {
				DestroyIcon(hIcon);
			}
		}
	}
	
	return windowIcon;
}

bool isWindowOnTop(HWND hwnd)
{
	if (!hwnd || !IsWindow(hwnd)) {
		return false;
	}
	
	// 获取窗口扩展样式
	LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	return (style & WS_EX_TOPMOST) != 0;
}

void setWindowOnTop(HWND hwnd, bool onTop)
{
	if (!hwnd || !IsWindow(hwnd)) {
		return;
	}
	
	// 设置窗口置顶状态
	SetWindowPos(hwnd,
		onTop ? HWND_TOPMOST : HWND_NOTOPMOST,
		0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	
	qDebug() << "Window" << QString::number(reinterpret_cast<qulonglong>(hwnd), 16)
		<< "set to" << (onTop ? "always on top" : "normal");
}

void flashWindowInTaskbar(HWND hwnd)
{
	if (!hwnd || !IsWindow(hwnd)) {
		return;
	}
	
	FlashWindow(hwnd, TRUE);
	
	qDebug() << "Window highlighted:" << QString::number(reinterpret_cast<qulonglong>(hwnd), 16);
}

} // namespace WindowInfoUtils