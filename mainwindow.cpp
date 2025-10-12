#include "mainwindow.h"
#include "translator.h"
#include "windowstraymanager.h"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <windows.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , trayIcon(nullptr)
    , trayMenu(nullptr)
    , showAction(nullptr)
    , restoreAllAction(nullptr)
    , quitAction(nullptr)
    , refreshTimer(nullptr)
{
    // 加载语言文件
    QString langFile = "./language/zh.lang";
    Translator::instance().loadLanguage(langFile);

    setWindowTitle(trc("MainWindow", "My Application - Window Manager"));
    resize(600, 500);

    // 设置窗口图标
    setWindowIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));

    // 初始化 Windows 原生托盘管理器
    if (!WindowsTrayManager::instance().initialize()) {
        QMessageBox::critical(this, trc("MainWindow", "Error"),
            trc("MainWindow", "Failed to initialize Windows tray manager"));
    }

    // 创建 UI
    setupUI();
    setupConnections();

    // 创建 Qt 托盘
    createTrayIcon();

    // 创建定时器用于自动刷新
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::refreshHiddenWindowsList);
    refreshTimer->start(1000); // 每1秒刷新一次

    // 初始隐藏主窗口
    hide();

    refreshAllWindowsList();
    refreshHiddenWindowsList();

    // 加载设置
    loadSettings();
}

MainWindow::~MainWindow()
{
    WindowsTrayManager::instance().shutdown();
}

void MainWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    tabWidget = new QTabWidget(centralWidget);

    // === 主页面 ===
    QWidget* mainTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(mainTab);

    // 标题
    QLabel* titleLabel = new QLabel(trc("MainWindow", "Window Process Manager"));
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; margin: 10px;");

    // 所有窗口列表组
    QGroupBox* allWindowsGroup = new QGroupBox(trc("MainWindow", "All Visible Windows"));
    QVBoxLayout* allWindowsLayout = new QVBoxLayout(allWindowsGroup);

    allWindowsList = new QListWidget();
    allWindowsList->setSelectionMode(QListWidget::SingleSelection);
    allWindowsLayout->addWidget(allWindowsList);

    // 所有窗口操作按钮
    QHBoxLayout* allWindowsButtonLayout = new QHBoxLayout();
    refreshAllButton = new QPushButton(trc("MainWindow", "Refresh All"));
    hideSelectedButton = new QPushButton(trc("MainWindow", "Hide Selected to Tray"));

    allWindowsButtonLayout->addWidget(refreshAllButton);
    allWindowsButtonLayout->addWidget(hideSelectedButton);
    allWindowsLayout->addLayout(allWindowsButtonLayout);

    // 所有窗口状态标签
    allWindowsStatusLabel = new QLabel(trc("MainWindow", "No windows found"));
    allWindowsStatusLabel->setStyleSheet("color: gray; font-style: italic;");
    allWindowsLayout->addWidget(allWindowsStatusLabel);

    // 隐藏窗口列表组（保留但可以缩小）
    QGroupBox* hiddenWindowsGroup = new QGroupBox(trc("MainWindow", "Hidden Windows (In Tray)"));
    QVBoxLayout* hiddenLayout = new QVBoxLayout(hiddenWindowsGroup);

    hiddenWindowsList = new QListWidget();
    hiddenWindowsList->setSelectionMode(QListWidget::SingleSelection);
    hiddenLayout->addWidget(hiddenWindowsList);

    QHBoxLayout* hiddenButtonLayout = new QHBoxLayout();
    refreshButton = new QPushButton(trc("MainWindow", "Refresh Hidden"));
    restoreSelectedButton = new QPushButton(trc("MainWindow", "Restore Selected"));
    restoreAllButton = new QPushButton(trc("MainWindow", "Restore All"));

    hiddenButtonLayout->addWidget(refreshButton);
    hiddenButtonLayout->addWidget(restoreSelectedButton);
    hiddenButtonLayout->addWidget(restoreAllButton);
    hiddenLayout->addLayout(hiddenButtonLayout);

    statusLabel = new QLabel(trc("MainWindow", "No windows are currently hidden"));
    statusLabel->setStyleSheet("color: gray; font-style: italic;");
    hiddenLayout->addWidget(statusLabel);

    // 快速操作区域（保留）
    QGroupBox* quickActionsGroup = new QGroupBox(trc("MainWindow", "Quick Actions"));
    QVBoxLayout* quickLayout = new QVBoxLayout(quickActionsGroup);

    QPushButton* minimizeCurrentBtn = new QPushButton(trc("MainWindow", "Minimize Current Active Window to Tray"));
    QLabel* hotkeyLabel = new QLabel(trc("MainWindow", "Hotkey: Win + Shift + Z"));
    hotkeyLabel->setStyleSheet("color: blue;");

    quickLayout->addWidget(minimizeCurrentBtn);
    quickLayout->addWidget(hotkeyLabel);

    // 组装主页面 - 改为上下布局
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(allWindowsGroup, 2);  // 更大的权重
    mainLayout->addWidget(hiddenWindowsGroup, 1); // 较小的权重
    mainLayout->addWidget(quickActionsGroup);
    mainLayout->addStretch();

    // === 设置页面 ===
    QWidget* settingsTab = new QWidget();
    QVBoxLayout* settingsLayout = new QVBoxLayout(settingsTab);

    QLabel* settingsTitle = new QLabel(trc("MainWindow", "Application Settings"));
    settingsTitle->setStyleSheet("font-size: 16px; font-weight: bold; margin: 10px;");

    // 常规设置
    QGroupBox* generalGroup = new QGroupBox(trc("MainWindow", "General Settings"));
    QVBoxLayout* generalLayout = new QVBoxLayout(generalGroup);

    startWithSystemCheck = new QCheckBox(trc("MainWindow", "Start with Windows"));
    enableHotkeyCheck = new QCheckBox(trc("MainWindow", "Enable Hotkey (Win+Shift+Z)"));
    enableHotkeyCheck->setChecked(true);

    generalLayout->addWidget(startWithSystemCheck);
    generalLayout->addWidget(enableHotkeyCheck);

    QGroupBox* refreshGroup = new QGroupBox(trc("MainWindow", "Auto Refresh Settings"));
    QFormLayout* refreshLayout = new QFormLayout(refreshGroup);

    refreshIntervalSpin = new QSpinBox();
    refreshIntervalSpin->setRange(1, 60);
    refreshIntervalSpin->setValue(1);
    refreshIntervalSpin->setSuffix(trc("MainWindow", "seconds"));

    autoRefreshCheck = new QCheckBox(trc("MainWindow", "Enable auto refresh"));
    autoRefreshCheck->setChecked(true);

    refreshLayout->addRow(autoRefreshCheck);
    refreshLayout->addRow(trc("MainWindow", "Refresh interval:"), refreshIntervalSpin);

    // 窗口设置
    QGroupBox* windowGroup = new QGroupBox(trc("MainWindow", "Window Settings"));
    QFormLayout* windowLayout = new QFormLayout(windowGroup);

    maxWindowsSpin = new QSpinBox();
    maxWindowsSpin->setRange(1, 100);
    maxWindowsSpin->setValue(50);
    maxWindowsSpin->setSuffix(trc("MainWindow", " windows"));

    languageCombo = new QComboBox();
    languageCombo->addItem("English", "en");
    languageCombo->addItem("中文", "zh");
    languageCombo->addItem("日本語", "ja");

    windowLayout->addRow(trc("MainWindow", "Maximum hidden windows:"), maxWindowsSpin);
    windowLayout->addRow(trc("MainWindow", "Language:"), languageCombo);

    // 保存设置按钮
    saveSettingsButton = new QPushButton(trc("MainWindow", "Save Settings"));

    settingsLayout->addWidget(settingsTitle);
    settingsLayout->addWidget(generalGroup);
    settingsLayout->addWidget(refreshGroup);
    settingsLayout->addWidget(windowGroup);
    settingsLayout->addWidget(saveSettingsButton);
    settingsLayout->addStretch();

    // === 关于页面 ===
    QWidget* aboutTab = new QWidget();
    QVBoxLayout* aboutLayout = new QVBoxLayout(aboutTab);

    QLabel* aboutTitle = new QLabel(trc("MainWindow", "About"));
    aboutTitle->setStyleSheet("font-size: 16px; font-weight: bold; margin: 10px;");

    aboutLabel = new QLabel();
    aboutLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    aboutLabel->setWordWrap(true);

    QString aboutText = QString(
        "<h3>%1</h3>"
        "<p><b>%2:</b> 1.0.0</p>"
        "<p><b>%3:</b> A powerful window management tool that allows you to minimize windows to system tray.</p>"
        "<p><b>%4:</b> Win + Shift + Z - Minimize active window to tray</p>"
        "<p><b>%5:</b> Double-click tray icon to restore window</p>"
        "<hr>"
        "<p>%6</p>"
    ).arg(
        trc("MainWindow", "Window Tray Manager"),
        trc("MainWindow", "Version"),
        trc("MainWindow", "Description"),
        trc("MainWindow", "Hotkey"),
        trc("MainWindow", "Usage"),
        trc("MainWindow", "Thank you for using our application!")
    );

    aboutLabel->setText(aboutText);

    QPushButton* githubButton = new QPushButton(trc("MainWindow", "Visit GitHub Repository"));
    QPushButton* checkUpdateButton = new QPushButton(trc("MainWindow", "Check for Updates"));

    aboutLayout->addWidget(aboutTitle);
    aboutLayout->addWidget(aboutLabel);
    aboutLayout->addWidget(githubButton);
    aboutLayout->addWidget(checkUpdateButton);
    aboutLayout->addStretch();

    // 添加标签页
    tabWidget->addTab(mainTab, trc("MainWindow", "Main"));
    tabWidget->addTab(settingsTab, trc("MainWindow", "Settings"));
    tabWidget->addTab(aboutTab, trc("MainWindow", "About"));

    // 设置中心布局
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->addWidget(tabWidget);

    // 连接快速操作按钮
    connect(minimizeCurrentBtn, &QPushButton::clicked, this, &MainWindow::minimizeActiveToTray);
    connect(githubButton, &QPushButton::clicked, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/your-repo/window-tray-manager"));
        });
    connect(checkUpdateButton, &QPushButton::clicked, this, &MainWindow::showAbout);
}

void MainWindow::setupConnections()
{
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshHiddenWindowsList);
    connect(restoreSelectedButton, &QPushButton::clicked, this, &MainWindow::restoreSelectedWindow);
    connect(restoreAllButton, &QPushButton::clicked, this, &MainWindow::restoreAllWindows);
    connect(saveSettingsButton, &QPushButton::clicked, this, &MainWindow::updateSettings);
    connect(hiddenWindowsList, &QListWidget::itemDoubleClicked, this, &MainWindow::restoreSelectedWindow);
    connect(refreshAllButton, &QPushButton::clicked, this, &MainWindow::refreshAllWindowsList);
    connect(hideSelectedButton, &QPushButton::clicked, this, &MainWindow::hideSelectedToTray);
    connect(allWindowsList, &QListWidget::itemDoubleClicked, this, &MainWindow::hideSelectedToTray);
}

void MainWindow::refreshHiddenWindowsList()
{
    hiddenWindowsList->clear();

    // 从 WindowsTrayManager 获取真实的隐藏窗口列表
    auto hiddenWindows = WindowsTrayManager::instance().getHiddenWindows();

    for (const auto& window : hiddenWindows) {
        if (!window.first || !IsWindow(window.first)) {
            continue; // 跳过无效窗口
        }
        // 将宽字符串转换为 QString
        QString title = QString::fromWCharArray(window.second.c_str());
        if (title.isEmpty() || title == "Unknown Window") {
            title = trc("MainWindow", "Unknown Window");
        }

        // 创建列表项并存储窗口句柄
        QListWidgetItem* item = new QListWidgetItem(title);
        item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(window.first));
        hiddenWindowsList->addItem(item);
    }

    // 更新状态标签
    int hiddenCount = hiddenWindowsList->count();
    if (hiddenCount > 0) {
        statusLabel->setText(trc("MainWindow", "%1 windows are currently hidden").arg(hiddenCount));
        statusLabel->setStyleSheet("color: green;");
    }
    else {
        statusLabel->setText(trc("MainWindow", "No windows are currently hidden"));
        statusLabel->setStyleSheet("color: gray; font-style: italic;");
    }
}


void MainWindow::restoreSelectedWindow()
{
    QListWidgetItem* item = hiddenWindowsList->currentItem();
    if (!item) {
        QMessageBox::information(this, trc("MainWindow", "Information"),
            trc("MainWindow", "Please select a window to restore"));
        return;
    }

    // 获取窗口句柄
    HWND hwnd = reinterpret_cast<HWND>(item->data(Qt::UserRole).toULongLong());

    if (!hwnd || !IsWindow(hwnd)) {
        QMessageBox::warning(this, trc("MainWindow", "Warning"),
            trc("MainWindow", "The selected window is no longer available"));
        // 从列表中移除无效项
        delete item;
        refreshHiddenWindowsList();
        return;
    }

    // 调用 WindowsTrayManager 恢复单个窗口
    if (WindowsTrayManager::instance().restoreWindow(hwnd)) {
        // 成功恢复，从列表中移除
        delete item;
        refreshHiddenWindowsList();
    }
    else {
        QMessageBox::warning(this, trc("MainWindow", "Error"),
            trc("MainWindow", "Failed to restore the window"));
    }
}

void MainWindow::restoreAllWindows()
{
    WindowsTrayManager::instance().restoreAllWindows();
    refreshHiddenWindowsList();
}

void MainWindow::updateSettings()
{
    saveSettings();

    // 应用刷新设置
    if (autoRefreshCheck->isChecked()) {
        refreshTimer->start(refreshIntervalSpin->value() * 1000);
    }
    else {
        refreshTimer->stop();
    }
    // 应用设置（这里可以添加实时生效的逻辑）
    bool hotkeyEnabled = enableHotkeyCheck->isChecked();
    // TODO: 实际启用/禁用热键

    QString language = languageCombo->currentData().toString();
    // TODO: 重新加载语言文件

    QMessageBox::information(this, trc("MainWindow", "Success"),
        trc("MainWindow", "Settings saved successfully!"));
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, trc("MainWindow", "About"),
        trc("MainWindow",
            "<h3>Window Tray Manager</h3>"
            "<p>Version: 1.0.0</p>"
            "<p>A powerful tool to manage your windows efficiently.</p>"
            "<p>Features:</p>"
            "<ul>"
            "<li>Minimize windows to system tray</li>"
            "<li>Restore windows with double-click</li>"
            "<li>Hotkey support (Win+Shift+Z)</li>"
            "<li>Multiple window management</li>"
            "</ul>"
        ));
}

QString MainWindow::trc(const char* context, const char* source) const
{
    return Translator::instance().translate(QString::fromUtf8(context), QString::fromUtf8(source));
}

void MainWindow::minimizeActiveToTray()
{
    HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow && foregroundWindow != (HWND)winId()) {
        if (WindowsTrayManager::instance().minimizeWindowToTray(foregroundWindow)) {
            refreshHiddenWindowsList(); // 刷新列表
        }
    }
}

void MainWindow::showWindow()
{
    show();
    raise();
    activateWindow();
    refreshAllWindowsList();
    refreshHiddenWindowsList();

    // 确保定时器运行
    if (refreshTimer && !refreshTimer->isActive()) {
        refreshTimer->start(1000);
    }
}

void MainWindow::closeApp()
{
    WindowsTrayManager::instance().shutdown();
    qApp->quit();
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        showWindow();
        break;
    case QSystemTrayIcon::Context:
        break;
    default:
        break;
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (trayIcon && trayIcon->isVisible()) {
        hide();
        event->ignore();
        // 隐藏时停止定时器以节省资源
        if (refreshTimer && refreshTimer->isActive()) {
            refreshTimer->stop();
        }
    }
    else {
        WindowsTrayManager::instance().shutdown();
        if (refreshTimer) {
            refreshTimer->stop();
        }
        QMainWindow::closeEvent(event);
    }
}

void MainWindow::createTrayIcon()
{
    // 创建菜单
    trayMenu = new QMenu(this);

    showAction = new QAction(trc("MainWindow", "Open Main Window"), this);
    connect(showAction, &QAction::triggered, this, &MainWindow::showWindow);

    restoreAllAction = new QAction(trc("MainWindow", "Restore All Windows"), this);
    connect(restoreAllAction, &QAction::triggered, this, &MainWindow::restoreAllWindows);

    quitAction = new QAction(trc("MainWindow", "Exit"), this);
    connect(quitAction, &QAction::triggered, this, &MainWindow::closeApp);

    trayMenu->addAction(showAction);
    trayMenu->addSeparator();
    trayMenu->addAction(restoreAllAction);
    trayMenu->addSeparator();
    trayMenu->addAction(quitAction);

    // 创建托盘图标
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayMenu);
    trayIcon->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    trayIcon->setToolTip(trc("MainWindow", "Window Tray Manager - Right click for menu"));
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    trayIcon->show();
}

QString MainWindow::getConfigPath() const
{
    // 使用程序目录下的 config.ini
    return QCoreApplication::applicationDirPath() + "/config.ini";
}

void MainWindow::loadSettings()
{
    QSettings settings(getConfigPath(), QSettings::IniFormat);

    // 热键设置
    bool hotkeyEnabled = settings.value("hotkey/enabled", true).toBool();
    enableHotkeyCheck->setChecked(hotkeyEnabled);

    // 窗口设置
    int maxWindows = settings.value("window/max_hidden", 50).toInt();
    maxWindowsSpin->setValue(maxWindows);

    // 常规设置
    bool startWithSystem = settings.value("general/start_with_system", false).toBool();
    startWithSystemCheck->setChecked(startWithSystem);

    QString language = settings.value("general/language", "zh").toString();
    int index = languageCombo->findData(language);
    if (index >= 0) {
        languageCombo->setCurrentIndex(index);
    }

    // 刷新设置
    bool autoRefresh = settings.value("refresh/auto_refresh", true).toBool();
    autoRefreshCheck->setChecked(autoRefresh);
    int refreshInterval = settings.value("refresh/interval", 1).toInt();
    refreshIntervalSpin->setValue(refreshInterval);

    // 应用刷新设置到定时器
    if (autoRefresh) {
        refreshTimer->start(refreshInterval * 1000);
    }
    else {
        refreshTimer->stop();
    }

    qDebug() << "Settings loaded from:" << getConfigPath();
}

void MainWindow::saveSettings()
{
    QSettings settings(getConfigPath(), QSettings::IniFormat);

    // 热键设置
    settings.setValue("hotkey/enabled", enableHotkeyCheck->isChecked());

    // 窗口设置
    settings.setValue("window/max_hidden", maxWindowsSpin->value());

    // 常规设置
    settings.setValue("general/start_with_system", startWithSystemCheck->isChecked());
    settings.setValue("general/language", languageCombo->currentData().toString());

    // 刷新设置
    settings.setValue("refresh/auto_refresh", autoRefreshCheck->isChecked());
    settings.setValue("refresh/interval", refreshIntervalSpin->value());

    settings.sync(); // 立即写入磁盘

    qDebug() << "Settings saved to:" << getConfigPath();
}

void MainWindow::refreshAllWindowsList()
{
    allWindowsList->clear();

    auto visibleWindows = getAllVisibleWindows();

    for (const auto& window : visibleWindows) {
        HWND hwnd = window.first;
        QString title = window.second;

        // 检查窗口是否已经被隐藏
        bool isHidden = false;
        auto hiddenWindows = WindowsTrayManager::instance().getHiddenWindows();
        for (const auto& hiddenWindow : hiddenWindows) {
            if (hiddenWindow.first == hwnd) {
                isHidden = true;
                break;
            }
        }

        // 创建列表项
        QString itemText = title;
        if (isHidden) {
            itemText += " [Hidden in Tray]";
        }

        QListWidgetItem* item = new QListWidgetItem(itemText);
        item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(hwnd));

        if (isHidden) {
            item->setForeground(Qt::gray);
            item->setToolTip(trc("MainWindow", "This window is currently hidden in tray"));
        }

        allWindowsList->addItem(item);
    }

    // 更新状态标签
    int windowCount = allWindowsList->count();
    if (windowCount > 0) {
        allWindowsStatusLabel->setText(trc("MainWindow", "Found %1 windows").arg(windowCount));
        allWindowsStatusLabel->setStyleSheet("color: green;");
    }
    else {
        allWindowsStatusLabel->setText(trc("MainWindow", "No windows found"));
        allWindowsStatusLabel->setStyleSheet("color: gray; font-style: italic;");
    }
}

void MainWindow::hideSelectedToTray()
{
    QListWidgetItem* item = allWindowsList->currentItem();
    if (!item) {
        QMessageBox::information(this, trc("MainWindow", "Information"),
            trc("MainWindow", "Please select a window to hide"));
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(item->data(Qt::UserRole).toULongLong());

    if (WindowsTrayManager::instance().minimizeWindowToTray(hwnd)) {
        // 成功隐藏，刷新两个列表
        refreshAllWindowsList();
        refreshHiddenWindowsList();

        QMessageBox::information(this, trc("MainWindow", "Success"),
            trc("MainWindow", "Window hidden to tray successfully"));
    }
    else {
        QMessageBox::warning(this, trc("MainWindow", "Error"),
            trc("MainWindow", "Failed to hide window to tray"));
    }
}

QList<QPair<HWND, QString>> MainWindow::getAllVisibleWindows() const
{
    QList<QPair<HWND, QString>> windows;

    // 使用 EnumWindows 枚举所有顶层窗口
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto windowsList = reinterpret_cast<QList<QPair<HWND, QString>>*>(lParam);

        // 检查窗口是否可见
        if (IsWindowVisible(hwnd)) {
            wchar_t title[256];
            if (GetWindowText(hwnd, title, 256) > 0) {
                QString windowTitle = QString::fromWCharArray(title);
                // 过滤掉空标题和系统窗口
                if (!windowTitle.isEmpty() && windowTitle != "Program Manager") {
                    windowsList->append(qMakePair(hwnd, windowTitle));
                }
            }
        }
        return TRUE;
        }, reinterpret_cast<LPARAM>(&windows));

    return windows;
}