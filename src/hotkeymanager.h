#pragma once

#include <QObject>
#include <QHash>
#include <QKeySequence>
#include <QSettings>
#include <windows.h>

class HotkeyManager : public QObject
{
    Q_OBJECT

public:
    static HotkeyManager& instance();

    // 注册热键
    bool registerHotkey(const QString& id, const QKeySequence& keySequence);

    // 注销热键
    bool unregisterHotkey(const QString& id);

    // 注销所有热键
    void unregisterAll();

    // 检查热键是否已注册
    bool isHotkeyRegistered(const QString& id) const;

    // 获取热键的虚拟键码和修饰符
    static bool parseKeySequence(const QKeySequence& keySequence, UINT& modifiers, UINT& key);

    // 保存/加载热键配置
    void saveHotkeys(QSettings& settings);
    void loadHotkeys(QSettings& settings);

    // 获取所有已注册的热键
    QHash<QString, QKeySequence> getAllHotkeys() const;

signals:
    void hotkeyTriggered(const QString& id);

private:
    HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager();

    static LRESULT CALLBACK hotkeyWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    bool createHotkeyWindow();
    void destroyHotkeyWindow();

private:
    HWND m_hotkeyWindow = nullptr;
    QHash<QString, int> m_hotkeyIds;  // id -> hotkey id
    QHash<int, QString> m_idToHotkey; // hotkey id -> id
    QHash<QString, QKeySequence> m_hotkeys; // id -> key sequence

    static HotkeyManager* s_instance;
    static const int BASE_HOTKEY_ID = 1000;
    int m_nextHotkeyId = BASE_HOTKEY_ID;
};