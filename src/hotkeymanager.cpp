#include "hotkeymanager.h"
#include <QDebug>
#include <QApplication>
#include <objbase.h>

HotkeyManager* HotkeyManager::s_instance = nullptr;

HotkeyManager& HotkeyManager::instance()
{
	if (!s_instance) {
		s_instance = new HotkeyManager();
	}
	return *s_instance;
}

HotkeyManager::HotkeyManager(QObject* parent)
	: QObject(parent)
{
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	createHotkeyWindow();
}

HotkeyManager::~HotkeyManager()
{
	unregisterAll();
	destroyHotkeyWindow();

	CoUninitialize();

	if (s_instance) {
		delete s_instance;
		s_instance = nullptr;
	}
}

bool HotkeyManager::createHotkeyWindow()
{
	// 注册窗口类
	WNDCLASS wc = {};
	wc.lpfnWndProc = hotkeyWndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = L"HotkeyManagerWindowClass";
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

	if (!RegisterClass(&wc)) {
		qWarning() << "Failed to register hotkey window class";
		return false;
	}

	// 创建消息窗口
	m_hotkeyWindow = CreateWindow(
		wc.lpszClassName,
		L"Hotkey Manager",
		0, 0, 0, 0, 0,
		HWND_MESSAGE,
		nullptr,
		GetModuleHandle(nullptr),
		this
	);

	if (!m_hotkeyWindow) {
		qWarning() << "Failed to create hotkey window";
		return false;
	}

	return true;
}

void HotkeyManager::destroyHotkeyWindow()
{
	if (m_hotkeyWindow) {
		DestroyWindow(m_hotkeyWindow);
		m_hotkeyWindow = nullptr;
	}
}

LRESULT CALLBACK HotkeyManager::hotkeyWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HotkeyManager* manager = nullptr;

	if (uMsg == WM_NCCREATE) {
		CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
		manager = reinterpret_cast<HotkeyManager*>(createStruct->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(manager));
	}
	else {
		manager = reinterpret_cast<HotkeyManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	if (manager && uMsg == WM_HOTKEY) {
		QString hotkeyId = manager->m_idToHotkey.value(wParam);
		if (!hotkeyId.isEmpty()) {
			emit manager->hotkeyTriggered(hotkeyId);
		}
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool HotkeyManager::parseKeySequence(const QKeySequence& keySequence, UINT& modifiers, UINT& key)
{
	if (keySequence.isEmpty()) {
		return false;
	}

	int keyCode = keySequence[0];
	modifiers = 0;
	key = 0;

	// 解析修饰键
	if (keyCode & Qt::ShiftModifier) {
		modifiers |= MOD_SHIFT;
	}
	if (keyCode & Qt::ControlModifier) {
		modifiers |= MOD_CONTROL;
	}
	if (keyCode & Qt::AltModifier) {
		modifiers |= MOD_ALT;
	}
	if (keyCode & Qt::MetaModifier) {
		modifiers |= MOD_WIN;
	}

	// 解析主键
	key = keyCode & ~Qt::KeyboardModifierMask;

	// 转换 Qt 键码到 Windows 虚拟键码
	switch (key) {
		case Qt::Key_A: key = 'A'; break;
		case Qt::Key_B: key = 'B'; break;
		case Qt::Key_C: key = 'C'; break;
		case Qt::Key_D: key = 'D'; break;
		case Qt::Key_E: key = 'E'; break;
		case Qt::Key_F: key = 'F'; break;
		case Qt::Key_G: key = 'G'; break;
		case Qt::Key_H: key = 'H'; break;
		case Qt::Key_I: key = 'I'; break;
		case Qt::Key_J: key = 'J'; break;
		case Qt::Key_K: key = 'K'; break;
		case Qt::Key_L: key = 'L'; break;
		case Qt::Key_M: key = 'M'; break;
		case Qt::Key_N: key = 'N'; break;
		case Qt::Key_O: key = 'O'; break;
		case Qt::Key_P: key = 'P'; break;
		case Qt::Key_Q: key = 'Q'; break;
		case Qt::Key_R: key = 'R'; break;
		case Qt::Key_S: key = 'S'; break;
		case Qt::Key_T: key = 'T'; break;
		case Qt::Key_U: key = 'U'; break;
		case Qt::Key_V: key = 'V'; break;
		case Qt::Key_W: key = 'W'; break;
		case Qt::Key_X: key = 'X'; break;
		case Qt::Key_Y: key = 'Y'; break;
		case Qt::Key_Z: key = 'Z'; break;
		case Qt::Key_0: key = '0'; break;
		case Qt::Key_1: key = '1'; break;
		case Qt::Key_2: key = '2'; break;
		case Qt::Key_3: key = '3'; break;
		case Qt::Key_4: key = '4'; break;
		case Qt::Key_5: key = '5'; break;
		case Qt::Key_6: key = '6'; break;
		case Qt::Key_7: key = '7'; break;
		case Qt::Key_8: key = '8'; break;
		case Qt::Key_9: key = '9'; break;
		case Qt::Key_F1: key = VK_F1; break;
		case Qt::Key_F2: key = VK_F2; break;
		case Qt::Key_F3: key = VK_F3; break;
		case Qt::Key_F4: key = VK_F4; break;
		case Qt::Key_F5: key = VK_F5; break;
		case Qt::Key_F6: key = VK_F6; break;
		case Qt::Key_F7: key = VK_F7; break;
		case Qt::Key_F8: key = VK_F8; break;
		case Qt::Key_F9: key = VK_F9; break;
		case Qt::Key_F10: key = VK_F10; break;
		case Qt::Key_F11: key = VK_F11; break;
		case Qt::Key_F12: key = VK_F12; break;
		case Qt::Key_Space: key = VK_SPACE; break;
		case Qt::Key_Enter: key = VK_RETURN; break;
		case Qt::Key_Return: key = VK_RETURN; break;
		case Qt::Key_Escape: key = VK_ESCAPE; break;
		case Qt::Key_Tab: key = VK_TAB; break;
		case Qt::Key_Backspace: key = VK_BACK; break;
		case Qt::Key_Delete: key = VK_DELETE; break;
		case Qt::Key_Insert: key = VK_INSERT; break;
		case Qt::Key_Home: key = VK_HOME; break;
		case Qt::Key_End: key = VK_END; break;
		case Qt::Key_PageUp: key = VK_PRIOR; break;
		case Qt::Key_PageDown: key = VK_NEXT; break;
		case Qt::Key_Up: key = VK_UP; break;
		case Qt::Key_Down: key = VK_DOWN; break;
		case Qt::Key_Left: key = VK_LEFT; break;
		case Qt::Key_Right: key = VK_RIGHT; break;
		default:
			qWarning() << "Unsupported key:" << key;
			return false;
	}

	return true;
}

bool HotkeyManager::registerHotkey(const QString& id, const QKeySequence& keySequence)
{
	if (!m_hotkeyWindow) {
		qWarning() << "Hotkey window not created";
		return false;
	}

	// 如果热键已注册，先注销
	if (m_hotkeyIds.contains(id)) {
		unregisterHotkey(id);
	}

	UINT modifiers, key;
	if (!parseKeySequence(keySequence, modifiers, key)) {
		qWarning() << "Failed to parse key sequence:" << keySequence.toString();
		return false;
	}

	int hotkeyId = m_nextHotkeyId++;
	if (RegisterHotKey(m_hotkeyWindow, hotkeyId, modifiers | MOD_NOREPEAT, key)) {
		m_hotkeyIds[id] = hotkeyId;
		m_idToHotkey[hotkeyId] = id;
		m_hotkeys[id] = keySequence;
		qDebug() << "Hotkey registered:" << id << "=" << keySequence.toString();
		return true;
	}
	else {
		qWarning() << "Failed to register hotkey:" << id << "=" << keySequence.toString();
		return false;
	}
}

bool HotkeyManager::unregisterHotkey(const QString& id)
{
	if (!m_hotkeyIds.contains(id)) {
		return false;
	}

	int hotkeyId = m_hotkeyIds[id];
	if (UnregisterHotKey(m_hotkeyWindow, hotkeyId)) {
		m_hotkeyIds.remove(id);
		m_idToHotkey.remove(hotkeyId);
		m_hotkeys.remove(id);
		qDebug() << "Hotkey unregistered:" << id;
		return true;
	}
	else {
		qWarning() << "Failed to unregister hotkey:" << id;
		return false;
	}
}

void HotkeyManager::unregisterAll()
{
	for (const QString& id : m_hotkeyIds.keys()) {
		unregisterHotkey(id);
	}
}

bool HotkeyManager::isHotkeyRegistered(const QString& id) const
{
	return m_hotkeyIds.contains(id);
}

void HotkeyManager::saveHotkeys(QSettings& settings)
{
	settings.beginGroup("Hotkeys");
	for (auto it = m_hotkeys.begin(); it != m_hotkeys.end(); ++it) {
		settings.setValue(it.key(), it.value().toString());
	}
	settings.endGroup();
}

void HotkeyManager::loadHotkeys(QSettings& settings)
{
	settings.beginGroup("Hotkeys");
	QStringList keys = settings.childKeys();
	for (const QString& key : keys) {
		QKeySequence keySequence = QKeySequence::fromString(settings.value(key).toString());
		if (!keySequence.isEmpty()) {
			registerHotkey(key, keySequence);
		}
	}
	settings.endGroup();
}

QHash<QString, QKeySequence> HotkeyManager::getAllHotkeys() const
{
	return m_hotkeys;
}

bool HotkeyManager::isSystemReservedHotkey(const QKeySequence& keySequence)
{
	if (keySequence.isEmpty()) {
		return false;
	}

	UINT modifiers, key;
	if (!parseKeySequence(keySequence, modifiers, key)) {
		return false;
	}

	// 检查常见 Windows 系统保留热键
	// 这些热键通常由 Windows 系统或常见应用程序使用

	// 1.检查是否包含 Win 键
	bool hasWinKey = (modifiers & MOD_WIN) != 0;

	if (hasWinKey) {
		// Windows 系统热键列表（部分）
		// Win + 字母/数字键
		if (key >= 'A' && key <= 'Z') {
			// 常见 Win 组合键
			switch (key) {
				case 'A': // Win+A - 操作中心
				case 'B': // Win+B - 快速切换通知区域
				case 'C': // Win+C - Cortana/Teams 聊天
				case 'D': // Win+D - 显示桌面
				case 'E': // Win+E - 文件资源管理器
				case 'F': // Win+F - 反馈中心
				case 'G': // Win+G - Xbox Game Bar
				case 'H': // Win+H - 听写
				case 'I': // Win+I - 设置
				case 'J': // Win+J - 焦点助手
				case 'K': // Win+K - 连接
				case 'L': // Win+L - 锁定电脑
				case 'M': // Win+M - 最小化所有窗口
				case 'O': // Win+O - 锁定设备方向
				case 'P': // Win+P - 投影模式
				case 'Q': // Win+Q - 搜索
				case 'R': // Win+R - 运行对话框
				case 'S': // Win+S - 搜索
				case 'T': // Win+T - 任务栏循环
				case 'U': // Win+U - 轻松使用设置中心
				case 'V': // Win+V - 剪贴板历史记录
				case 'W': // Win+W - 小工具
				case 'X': // Win+X - 快速链接菜单
				case 'Y': // Win+Y - 切换输入
				case 'Z': // Win+Z - 对齐布局（Windows 11）
				case '1': // Win+1-9 - 启动任务栏程序
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case '0': // Win+0 - 启动第10个任务栏程序
					return true;
			}
		}

		// Win + 功能键
		switch (key) {
			case VK_F1:  // Win+F1 - 帮助
			case VK_F3:  // Win+F3 - 搜索
			case VK_F4:  // Win+F4 - 关闭窗口
			case VK_F6:  // Win+F6 - 切换窗口
			case VK_TAB: // Win+Tab - 任务视图
			case VK_PAUSE: // Win+Pause - 系统属性
			case VK_PRINT: // Win+PrintScreen - 截屏保存
			case VK_LEFT:  // Win+Left - 窗口靠左
			case VK_RIGHT: // Win+Right - 窗口靠右
			case VK_UP:    // Win+Up - 最大化
			case VK_DOWN:  // Win+Down - 最小化/恢复
			case VK_HOME:  // Win+Home - 最小化非活动窗口
			case VK_SPACE: // Win+Space - 切换输入法
			case VK_RETURN: // Win+Enter - 讲述人
			case VK_OEM_PERIOD: // Win+. - 表情符号面板
			case VK_OEM_COMMA:  // Win+, - 桌面透视
			case VK_OEM_PLUS:   // Win++ - 放大
			case VK_OEM_MINUS:  // Win+- - 缩小
				return true;
		}

		// Win + Shift 组合
		if (modifiers & MOD_SHIFT) {
			switch (key) {
				case 'S': // Win+Shift+S - 截图工具
				case VK_LEFT:  // Win+Shift+Left - 移动到左侧显示器
				case VK_RIGHT: // Win+Shift+Right - 移动到右侧显示器
				case VK_UP:    // Win+Shift+Up - 窗口拉伸到顶部
				case VK_DOWN:  // Win+Shift+Down - 窗口恢复
					return true;
			}
		}

		// Win + Ctrl 组合
		if (modifiers & MOD_CONTROL) {
			switch (key) {
				case 'F': // Win+Ctrl+F - 查找计算机
				case 'D': // Win+Ctrl+D - 新建虚拟桌面
				case VK_LEFT:  // Win+Ctrl+Left - 切换到上一个虚拟桌面
				case VK_RIGHT: // Win+Ctrl+Right - 切换到下一个虚拟桌面
				case VK_F4:    // Win+Ctrl+F4 - 关闭当前虚拟桌面
					return true;
			}
		}

		// Win + Alt 组合
		if (modifiers & MOD_ALT) {
			switch (key) {
				case VK_RETURN: // Win+Alt+Enter - 媒体播放器全屏
				case 'D':      // Win+Alt+D - 显示日期时间
					return true;
			}
		}
	}

	// 2. 其他系统保留组合（非 Win 键）
	// Ctrl+Alt+Del - 安全选项（无法被应用程序捕获）
	if ((modifiers & MOD_CONTROL) && (modifiers & MOD_ALT) && key == VK_DELETE) {
		return true;
	}

	// Alt+F4 - 关闭窗口
	if ((modifiers & MOD_ALT) && key == VK_F4) {
		return true;
	}

	// Ctrl+Esc - 开始菜单
	if ((modifiers & MOD_CONTROL) && key == VK_ESCAPE) {
		return true;
	}

	// Alt+Tab - 切换窗口
	if ((modifiers & MOD_ALT) && key == VK_TAB) {
		return true;
	}

	// Alt+Space - 窗口菜单
	if ((modifiers & MOD_ALT) && key == VK_SPACE) {
		return true;
	}

	// F1 - 帮助
	if (key == VK_F1 && modifiers == 0) {
		return true;
	}

	// Ctrl+C, Ctrl+V, Ctrl+X, Ctrl+Z 等常用编辑快捷键
	// 这些通常可以被覆盖，但避免误操作
	if (modifiers == MOD_CONTROL) {
		switch (key) {
			case 'C': // 复制
			case 'V': // 粘贴
			case 'X': // 剪切
			case 'Z': // 撤销
			case 'Y': // 重做
			case 'A': // 全选
			case 'S': // 保存
			case 'P': // 打印
			case 'N': // 新建
			case 'O': // 打开
				return true;
		}
	}

	return false;
}