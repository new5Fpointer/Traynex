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

    // 新增功能槽函数
    void refreshHiddenWindowsList();
    void restoreSelectedWindow();
    void restoreAllWindows();
    void updateSettings();
    void showAbout();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createTrayIcon();
    void setupUI();
    void setupConnections();

    // UI 组件
    QTabWidget* tabWidget;

    // 主页面组件
    QListWidget* hiddenWindowsList;
    QPushButton* refreshButton;
    QPushButton* restoreSelectedButton;
    QPushButton* restoreAllButton;
    QLabel* statusLabel;

    // 设置页面组件
    QCheckBox* startWithSystemCheck;
    QCheckBox* enableHotkeyCheck;
    QSpinBox* maxWindowsSpin;
    QComboBox* languageCombo;
    QPushButton* saveSettingsButton;

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
};