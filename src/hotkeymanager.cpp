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

    if (manager) {
        if (uMsg == WM_HOTKEY) {
            qDebug() << "WM_HOTKEY received - wParam:" << wParam << "lParam:" << lParam;

            QString hotkeyId = manager->m_idToHotkey.value(wParam);
            if (!hotkeyId.isEmpty()) {
                qDebug() << "Hotkey triggered:" << hotkeyId;
                emit manager->hotkeyTriggered(hotkeyId);
            }
            else {
                qWarning() << "Unknown hotkey ID:" << wParam;
            }
            return 0;
        }
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

    qDebug() << "Registering hotkey - ID:" << id
        << "Sequence:" << keySequence.toString()
        << "Modifiers:" << modifiers
        << "Key:" << key;

    int hotkeyId = m_nextHotkeyId++;
    if (RegisterHotKey(m_hotkeyWindow, hotkeyId, modifiers | MOD_NOREPEAT, key)) {
        m_hotkeyIds[id] = hotkeyId;
        m_idToHotkey[hotkeyId] = id;
        m_hotkeys[id] = keySequence;
        qDebug() << "Hotkey registered successfully:" << id << "=" << keySequence.toString();
        return true;
    }
    else {
        DWORD error = GetLastError();
        qWarning() << "Failed to register hotkey:" << id
            << "Error code:" << error
            << "Sequence:" << keySequence.toString();
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