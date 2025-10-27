#pragma once

#include <QMainWindow>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QAction>
#include <QMenu>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QTimer>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    QString trc(const char* context, const char* source) const;

private slots:
    void minimizeActiveToTray();
    void showWindow();
    void closeApp();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void restoreSelectedWindow();
    void restoreAllWindows();
    void showAbout();
    void refreshAllLists();
    void hideSelectedToTray();
    void onTableContextMenu(const QPoint& pos);
    void bringToFront();
    void endTask();
    void onHotkeySettingChanged();
    void onRefreshSettingChanged();
    void onLanguageChanged();
    void onStartWithSystemChanged();
    void onMaxWindowsChanged();
    void autoSaveSettings();
    void onAlwaysOnTopChanged();
    void highlightWindow();
    void toggleWindowOnTop();
    void refreshHiddenWindowsTable();
    void restoreSelectedHiddenWindow();
    void onHiddenTableContextMenu(const QPoint& pos);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createTrayIcon();
    void setupUI();
    void setupConnections();
    void loadSettings();
    void saveSettings();

    void refreshWindowsTable();
    void createContextMenu();

    void loadLanguage(const QString& language);
    void retranslateUI();

    void updateWindowFlags();

    void flashWindowInTaskbar(HWND hwnd);

    bool isWindowOnTop(HWND hwnd);
    void setWindowOnTop(HWND hwnd, bool onTop);

    struct WindowInfo {
        QString title;
        QString processName;
        HWND hwnd;
        bool isHidden;
        bool isVisible;
    };
    QList<QPair<HWND, WindowInfo>> getAllWindowsInfo() const;
    QList<QPair<HWND, WindowInfo>> m_lastWindowsInfo;

    // 配置文件路径
    QString getConfigPath() const;
    HWND getSelectedWindow() const;

    // UI 组件
    QTabWidget* tabWidget;

    // 主页面组件
    QTableWidget* windowsTable;

    // 右键菜单
    QMenu* contextMenu;
    QAction* hideToTrayAction;
    QAction* bringToFrontAction;
    QAction* highlightAction;
    QAction* toggleOnTopAction;
    QAction* endTaskAction;

    // 隐藏窗口页面组件
    QTableWidget* hiddenWindowsTable;

    // 设置页面组件
    QCheckBox* startWithSystemCheck;
    QCheckBox* enableHotkeyCheck;
    QSpinBox* maxWindowsSpin;
    QComboBox* languageCombo;
    QPushButton* saveSettingsButton;
    QCheckBox* alwaysOnTopCheck;

    // 关于页面组件
    QLabel* aboutLabel;

    // 托盘相关
    QSystemTrayIcon* trayIcon;
    QMenu* trayMenu;
    QAction* showAction;
    QAction* restoreAllAction;
    QAction* quitAction;

    // 定时刷新计时器
    QTimer* refreshTimer;

    QCheckBox* autoRefreshCheck;
    QSpinBox* refreshIntervalSpin;
};