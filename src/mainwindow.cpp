#include "mainwindow.h"
#include "translator.h"
#include "windowstraymanager.h"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QHeaderView>
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
#include <QThread>
#include <windows.h>
#include <psapi.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , trayIcon(nullptr)
    , trayMenu(nullptr)
    , showAction(nullptr)
    , restoreAllAction(nullptr)
    , quitAction(nullptr)
    , refreshTimer(nullptr)
    , hiddenTableContextMenu(nullptr)
    , restoreHiddenAction(nullptr)
    , restoreAllHiddenAction(nullptr)
    , hideToAppTrayAction(nullptr)
{
    // 创建 UI
    setupUI();
    setupConnections();

    connect(&WindowsTrayManager::instance(), &WindowsTrayManager::trayWindowsChanged,
        this, &MainWindow::updateTrayMenu);
    connect(&WindowsTrayManager::instance(), &WindowsTrayManager::trayWindowsChanged,
        this, &MainWindow::refreshAllLists);

    // 创建定时器
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::refreshWindowsTable);
    refreshTimer->start(500);

    // 加载设置
    loadSettings();

    QString language = languageCombo->currentData().toString();
    loadLanguage(language);

    setWindowTitle("Traynex");
    resize(595, 500);

    // 初始化 Windows 原生托盘管理器
    if (!WindowsTrayManager::instance().initialize()) {
        QMessageBox::critical(this, trc("MainWindow", "Error"),
            trc("MainWindow", "Failed to initialize Windows tray manager"));
    }

    // 创建 Qt 托盘
    createTrayIcon();

    // 初始隐藏主窗口
    hide();

    refreshAllLists();
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

    // 标题和工具栏
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->addStretch();

    // 创建表格
    windowsTable = new QTableWidget();
    windowsTable->setColumnCount(3);
    windowsTable->setHorizontalHeaderLabels({
        trc("MainWindow", "Window Title"),
        trc("MainWindow", "Process"),
        trc("MainWindow", "Handle")
        });
    // 设置固定的行号列宽度
    windowsTable->verticalHeader()->setDefaultSectionSize(30); // 行高
    windowsTable->verticalHeader()->setMinimumWidth(20);       // 最小宽度
    windowsTable->verticalHeader()->setMaximumWidth(20);       // 最大宽度

    // 表格属性
    windowsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    windowsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    windowsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    windowsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    windowsTable->setSortingEnabled(true);

    // 列宽设置
    windowsTable->setColumnWidth(0, 300); // 标题
    windowsTable->setColumnWidth(1, 150); // 进程名
    windowsTable->setColumnWidth(2, 80);  // 句柄

    windowsTable->horizontalHeader()->setStyleSheet(
        "QHeaderView::section {"
        "    font-weight: normal;"
        "}"
    );

    // 组装布局
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(windowsTable);

    // 创建右键菜单
    createContextMenu();

    // 连接信号
    connect(windowsTable, &QTableWidget::customContextMenuRequested,
        this, &MainWindow::onTableContextMenu);

    // === 隐藏窗口页面 ===
    QWidget* hiddenTab = new QWidget();
    QVBoxLayout* hiddenLayout = new QVBoxLayout(hiddenTab);

    // 创建隐藏窗口表格
    hiddenWindowsTable = new QTableWidget();
    hiddenWindowsTable->setColumnCount(3);
    hiddenWindowsTable->setHorizontalHeaderLabels({
        trc("MainWindow", "Window Title"),
        trc("MainWindow", "Process"),
        trc("MainWindow", "Handle")
        });
    // 设置固定的行号列宽度
    hiddenWindowsTable->verticalHeader()->setDefaultSectionSize(30); // 行高
    hiddenWindowsTable->verticalHeader()->setMinimumWidth(20);       // 最小宽度
    hiddenWindowsTable->verticalHeader()->setMaximumWidth(20);       // 最大宽度

    // 表格属性
    hiddenWindowsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    hiddenWindowsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    hiddenWindowsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    hiddenWindowsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hiddenWindowsTable->setSortingEnabled(true);

    // 列宽设置
    hiddenWindowsTable->setColumnWidth(0, 300); // 标题
    hiddenWindowsTable->setColumnWidth(1, 150); // 进程名
    hiddenWindowsTable->setColumnWidth(2, 80);  // 句柄

    hiddenWindowsTable->horizontalHeader()->setStyleSheet(
        "QHeaderView::section {"
        "    font-weight: normal;"
        "}"
    );

    // 组装布局
    hiddenLayout->addWidget(hiddenWindowsTable);

    // 连接信号
    connect(hiddenWindowsTable, &QTableWidget::customContextMenuRequested,
        this, &MainWindow::onHiddenTableContextMenu);

    // === 设置页面 ===
    QWidget* settingsTab = new QWidget();
    QVBoxLayout* settingsLayout = new QVBoxLayout(settingsTab);

    // 常规设置
    QGroupBox* generalGroup = new QGroupBox(trc("MainWindow", "General Settings"));
    generalGroup->setObjectName("generalGroup"); // 设置对象名称
    QVBoxLayout* generalLayout = new QVBoxLayout(generalGroup);

    startWithSystemCheck = new QCheckBox(trc("MainWindow", "Start with Windows"));
    enableHotkeyCheck = new QCheckBox(trc("MainWindow", "Enable Hotkey (Win+Shift+Z)"));
    enableHotkeyCheck->setChecked(true);

    alwaysOnTopCheck = new QCheckBox(trc("MainWindow", "Always on Top"));
    alwaysOnTopCheck->setToolTip(trc("MainWindow", "Keep the main window always on top of other windows"));

    generalLayout->addWidget(startWithSystemCheck);
    generalLayout->addWidget(enableHotkeyCheck);
    generalLayout->addWidget(alwaysOnTopCheck);

    // 自动刷新设置
    QGroupBox* refreshGroup = new QGroupBox(trc("MainWindow", "Auto Refresh Settings"));
    refreshGroup->setObjectName("refreshGroup"); // 设置对象名称
    QFormLayout* refreshLayout = new QFormLayout(refreshGroup);

    refreshIntervalSpin = new QSpinBox();
    refreshIntervalSpin->setRange(100, 1000);
    refreshIntervalSpin->setValue(500);
    refreshIntervalSpin->setSuffix(trc("MainWindow", "ms"));

    autoRefreshCheck = new QCheckBox(trc("MainWindow", "Enable auto refresh"));
    autoRefreshCheck->setChecked(true);

    // 创建刷新间隔标签并设置对象名称
    QLabel* refreshIntervalLabel = new QLabel(trc("MainWindow", "Refresh interval:"));
    refreshIntervalLabel->setObjectName("refreshIntervalLabel");

    refreshLayout->addRow(autoRefreshCheck);
    refreshLayout->addRow(refreshIntervalLabel, refreshIntervalSpin);

    // 窗口设置
    QGroupBox* windowGroup = new QGroupBox(trc("MainWindow", "Window Settings"));
    windowGroup->setObjectName("windowGroup"); // 设置对象名称
    QFormLayout* windowLayout = new QFormLayout(windowGroup);

    maxWindowsSpin = new QSpinBox();
    maxWindowsSpin->setRange(1, 100);
    maxWindowsSpin->setValue(50);
    maxWindowsSpin->setSuffix(trc("MainWindow", " windows"));

    languageCombo = new QComboBox();
    languageCombo->addItem("English", "en");
    languageCombo->addItem("中文", "zh");

    // 创建表单标签并设置对象名称
    QLabel* maxWindowsLabel = new QLabel(trc("MainWindow", "Maximum hidden windows:"));
    maxWindowsLabel->setObjectName("maxWindowsLabel");

    QLabel* languageLabel = new QLabel(trc("MainWindow", "Language:"));
    languageLabel->setObjectName("languageLabel");

    windowLayout->addRow(maxWindowsLabel, maxWindowsSpin);
    windowLayout->addRow(languageLabel, languageCombo);

    settingsLayout->addWidget(generalGroup);
    settingsLayout->addWidget(refreshGroup);
    settingsLayout->addWidget(windowGroup);
    settingsLayout->addStretch();

    // === 关于页面 ===
    QWidget* aboutTab = new QWidget();
    QVBoxLayout* aboutLayout = new QVBoxLayout(aboutTab);

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
        trc("MainWindow", "Traynex"),
        trc("MainWindow", "Version"),
        trc("MainWindow", "Description"),
        trc("MainWindow", "Hotkey"),
        trc("MainWindow", "Usage"),
        trc("MainWindow", "Thank you for using Traynex!")
    );

    aboutLabel->setText(aboutText);

    QPushButton* githubButton = new QPushButton(trc("MainWindow", "Visit GitHub Repository"));
    githubButton->setObjectName("githubButton");

    QPushButton* checkUpdateButton = new QPushButton(trc("MainWindow", "Check for Updates"));
    checkUpdateButton->setObjectName("checkUpdateButton");

    aboutLayout->addWidget(aboutLabel);
    aboutLayout->addWidget(githubButton);
    aboutLayout->addWidget(checkUpdateButton);
    aboutLayout->addStretch();

    // 添加标签页
    tabWidget->addTab(mainTab, trc("MainWindow", "Main"));
    tabWidget->addTab(hiddenTab, trc("MainWindow", "Hidden Windows"));
    tabWidget->addTab(settingsTab, trc("MainWindow", "Settings"));
    tabWidget->addTab(aboutTab, trc("MainWindow", "About"));

    // 设置中心布局
    QVBoxLayout* centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->addWidget(tabWidget);

    // 连接快速操作按钮
    connect(githubButton, &QPushButton::clicked, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/new5Fpointer/Traynex"));
        });
    connect(checkUpdateButton, &QPushButton::clicked, this, &MainWindow::showAbout);
}

void MainWindow::setupConnections()
{
    connect(enableHotkeyCheck, &QCheckBox::stateChanged, this, &MainWindow::onHotkeySettingChanged);
    connect(autoRefreshCheck, &QCheckBox::stateChanged, this, &MainWindow::onRefreshSettingChanged);
    connect(refreshIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onRefreshSettingChanged);
    connect(languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onLanguageChanged);
    connect(startWithSystemCheck, &QCheckBox::stateChanged, this, &MainWindow::onStartWithSystemChanged);
    connect(maxWindowsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onMaxWindowsChanged);
    connect(alwaysOnTopCheck, &QCheckBox::stateChanged, this, &MainWindow::onAlwaysOnTopChanged);
}

void MainWindow::restoreSelectedWindow()
{
    HWND hwnd = getSelectedWindow();
    if (!hwnd) {
        QMessageBox::information(this, trc("MainWindow", "Information"),
            trc("MainWindow", "Please select a window to restore"));
        return;
    }

    if (!hwnd || !IsWindow(hwnd)) {
        QMessageBox::warning(this, trc("MainWindow", "Warning"),
            trc("MainWindow", "The selected window is no longer available"));
        refreshAllLists();
        return;
    }

    if (WindowsTrayManager::instance().restoreWindow(hwnd)) {
        refreshAllLists();
        updateTrayMenu();
    }
    else {
        QMessageBox::warning(this, trc("MainWindow", "Error"),
            trc("MainWindow", "Failed to restore the window"));
    }
}

void MainWindow::restoreAllWindows()
{
    // 恢复系统托盘隐藏的窗口
    WindowsTrayManager::instance().restoreAllWindows();

    // 恢复应用托盘菜单隐藏的窗口
    QList<HWND> appTrayWindows = m_appTrayWindows.keys();
    for (HWND hwnd : appTrayWindows) {
        if (hwnd && IsWindow(hwnd)) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        }
        removeWindowFromTrayMenu(hwnd);
    }

    refreshAllLists();
    updateTrayMenu();
}

void MainWindow::showAbout()
{
    if (aboutLabel) {
        QString aboutText = QString(
            "<h3>%1</h3>"
            "<p><b>%2:</b> 1.0.0</p>"
            "<p><b>%3:</b> A powerful window management tool that allows you to minimize windows to system tray.</p>"
            "<p><b>%4:</b> Win + Shift + Z - Minimize active window to tray</p>"
            "<p><b>%5:</b> Double-click tray icon to restore window</p>"
            "<hr>"
            "<p>%6</p>"
        ).arg(
            trc("MainWindow", "Traynex"),
            trc("MainWindow", "Version"),
            trc("MainWindow", "Description"),
            trc("MainWindow", "Hotkey"),
            trc("MainWindow", "Usage"),
            trc("MainWindow", "Thank you for using Traynex!")
        );
        aboutLabel->setText(aboutText);
    }
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
            refreshAllLists();
            updateTrayMenu();
        }
    }
}

void MainWindow::showWindow()
{
    updateWindowFlags();

    show();
    raise();
    activateWindow();
    refreshAllLists();

    if (autoRefreshCheck->isChecked() && refreshTimer && !refreshTimer->isActive()) {
        refreshTimer->start(refreshIntervalSpin->value());
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

    // 设置图标
    QIcon icon(":/icon/icon.png");
    if (icon.isNull()) {
        qWarning() << "Failed to load tray icon from resource, using default";
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    trayIcon->setIcon(icon);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->setToolTip(tr("Traynex"));

    // 连接信号
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);

    updateTrayMenu();

    // 显示托盘图标
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

    // 加载窗口置顶设置
    bool alwaysOnTop = settings.value("window/always_on_top", false).toBool();
    alwaysOnTopCheck->setChecked(alwaysOnTop);

    QString language = settings.value("general/language", "zh").toString();
    int index = languageCombo->findData(language);
    if (index >= 0) {
        languageCombo->setCurrentIndex(index);
    }

    // 刷新设置
    bool autoRefresh = settings.value("refresh/auto_refresh", true).toBool();
    autoRefreshCheck->setChecked(autoRefresh);
    int refreshInterval = settings.value("refresh/interval", 500).toInt();
    refreshIntervalSpin->setValue(refreshInterval);

    // 应用加载的设置
    onHotkeySettingChanged();     // 应用热键设置
    onRefreshSettingChanged();    // 应用刷新设置
    onAlwaysOnTopChanged();       // 应用置顶设置

    qDebug() << "Settings loaded and applied from:" << getConfigPath();
}

void MainWindow::saveSettings()
{
    QSettings settings(getConfigPath(), QSettings::IniFormat);

    // 热键设置
    settings.setValue("hotkey/enabled", enableHotkeyCheck->isChecked());

    // 窗口设置
    settings.setValue("window/max_hidden", maxWindowsSpin->value());
    settings.setValue("window/always_on_top", alwaysOnTopCheck->isChecked());

    // 常规设置
    settings.setValue("general/start_with_system", startWithSystemCheck->isChecked());
    settings.setValue("general/language", languageCombo->currentData().toString());

    // 刷新设置
    settings.setValue("refresh/auto_refresh", autoRefreshCheck->isChecked());
    settings.setValue("refresh/interval", refreshIntervalSpin->value());

    settings.sync(); // 立即写入磁盘

    qDebug() << "Settings saved to:" << getConfigPath();
}

void MainWindow::hideSelectedToTray()
{
    HWND hwnd = getSelectedWindow();
    if (!hwnd) {
        QMessageBox::information(this, trc("MainWindow", "Information"),
            trc("MainWindow", "Please select a window to hide"));
        return;
    }

    if (WindowsTrayManager::instance().minimizeWindowToTray(hwnd)) {
        // 成功隐藏，刷新显示
        refreshAllLists();
        updateTrayMenu();
        QMessageBox::information(this, trc("MainWindow", "Success"),
            trc("MainWindow", "Window hidden to tray successfully"));
    }
    else {
        QMessageBox::warning(this, trc("MainWindow", "Error"),
            trc("MainWindow", "Failed to hide window to tray"));
    }
}

void MainWindow::createContextMenu()
{
    contextMenu = new QMenu(this);

    hideToTrayAction = new QAction(trc("MainWindow", "Hide to Tray Icon"), this);
    hideToAppTrayAction = new QAction(trc("MainWindow", "Hide to Tray Menu"), this);
    bringToFrontAction = new QAction(trc("MainWindow", "Bring to Front"), this);
    highlightAction = new QAction(trc("MainWindow", "Highlight Window"), this);
    toggleOnTopAction = new QAction(trc("MainWindow", "Always on Top"), this);
    endTaskAction = new QAction(trc("MainWindow", "End Task"), this);

    toggleOnTopAction->setCheckable(true);

    connect(hideToTrayAction, &QAction::triggered, this, &MainWindow::hideSelectedToTray);
    connect(hideToAppTrayAction, &QAction::triggered, this, &MainWindow::hideToAppTray);
    connect(bringToFrontAction, &QAction::triggered, this, &MainWindow::bringToFront);
    connect(highlightAction, &QAction::triggered, this, &MainWindow::highlightWindow);
    connect(toggleOnTopAction, &QAction::triggered, this, &MainWindow::toggleWindowOnTop);
    connect(endTaskAction, &QAction::triggered, this, &MainWindow::endTask);

    contextMenu->addAction(hideToTrayAction);
    contextMenu->addAction(hideToAppTrayAction);
    contextMenu->addSeparator();
    contextMenu->addAction(bringToFrontAction);
    contextMenu->addAction(highlightAction);
    contextMenu->addAction(toggleOnTopAction);
    contextMenu->addSeparator();
    contextMenu->addAction(endTaskAction);
}

void MainWindow::onTableContextMenu(const QPoint& pos)
{
    // 临时停止自动刷新
    if (refreshTimer && refreshTimer->isActive()) {
        refreshTimer->stop();
    }

    // 获取点击位置对应的行
    int row = windowsTable->rowAt(pos.y());
    if (row < 0 || row >= windowsTable->rowCount()) {
        // 恢复计时器
        if (refreshTimer && autoRefreshCheck->isChecked()) {
            refreshTimer->start(refreshIntervalSpin->value());
        }
        return;
    }

    // 确保选中这一行
    windowsTable->setCurrentCell(row, 0);

    // 获取 HWND 数据
    QTableWidgetItem* hwndItem = windowsTable->item(row, 0);
    if (!hwndItem) {
        // 恢复计时器
        if (refreshTimer && autoRefreshCheck->isChecked()) {
            refreshTimer->start(refreshIntervalSpin->value());
        }
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(hwndItem->data(Qt::UserRole).toULongLong());
    if (!hwnd || !IsWindow(hwnd)) {
        // 恢复计时器
        if (refreshTimer && autoRefreshCheck->isChecked()) {
            refreshTimer->start(refreshIntervalSpin->value());
        }
        return;
    }

    // 根据窗口状态更新菜单项
    bool isHidden = false;
    auto hiddenWindows = WindowsTrayManager::instance().getHiddenWindows();
    for (const auto& hidden : hiddenWindows) {
        if (hidden.first == hwnd) {
            isHidden = true;
            break;
        }
    }
    bool isOnTop = isWindowOnTop(hwnd);

    hideToTrayAction->setEnabled(!isHidden);
    bringToFrontAction->setEnabled(true);
    highlightAction->setEnabled(true);
    toggleOnTopAction->setEnabled(true);
    endTaskAction->setEnabled(true);

    toggleOnTopAction->setChecked(isOnTop);

    // 显示菜单
    contextMenu->exec(windowsTable->viewport()->mapToGlobal(pos));

    // 恢复自动刷新
    if (refreshTimer && autoRefreshCheck->isChecked()) {
        refreshTimer->start(refreshIntervalSpin->value());
    }
}

void MainWindow::refreshWindowsTable()
{
    auto currentWindowsInfo = getAllWindowsInfo();

    // 检查窗口列表是否发生变化
    bool needsRefresh = false;

    if (currentWindowsInfo.size() != m_lastWindowsInfo.size()) {
        needsRefresh = true;
    }
    else {
        // 检查窗口状态是否有变化
        for (int i = 0; i < currentWindowsInfo.size(); ++i) {
            if (currentWindowsInfo[i].first != m_lastWindowsInfo[i].first ||
                currentWindowsInfo[i].second.isHidden != m_lastWindowsInfo[i].second.isHidden ||
                currentWindowsInfo[i].second.title != m_lastWindowsInfo[i].second.title ||
                currentWindowsInfo[i].second.processName != m_lastWindowsInfo[i].second.processName) {
                needsRefresh = true;
                break;
            }
        }
    }

    if (!needsRefresh) {
        return; // 没有变化，不刷新
    }

    // 保存当前选中的窗口句柄
    HWND previouslySelectedHwnd = getSelectedWindow();

    windowsTable->setSortingEnabled(false);
    windowsTable->setRowCount(0);

    for (const auto& window : currentWindowsInfo) {
        int row = windowsTable->rowCount();
        windowsTable->insertRow(row);

        // 窗口标题
        QTableWidgetItem* titleItem = new QTableWidgetItem(window.second.title);
        titleItem->setData(Qt::UserRole, reinterpret_cast<qulonglong>(window.second.hwnd));

        // 进程名
        QTableWidgetItem* processItem = new QTableWidgetItem(window.second.processName);

        // 窗口句柄
        QTableWidgetItem* handleItem = new QTableWidgetItem(
            QString::number(reinterpret_cast<qulonglong>(window.second.hwnd), 16).toUpper());

        windowsTable->setItem(row, 0, titleItem);
        windowsTable->setItem(row, 1, processItem);
        windowsTable->setItem(row, 2, handleItem);

        // 隐藏窗口显示为灰色
        if (window.second.isHidden) {
            for (int col = 0; col < 3; ++col) {
                if (auto item = windowsTable->item(row, col)) {
                    item->setForeground(Qt::gray);
                }
            }
        }

        // 恢复选中状态
        if (window.second.hwnd == previouslySelectedHwnd) {
            windowsTable->setCurrentCell(row, 0);
        }
    }

    windowsTable->setSortingEnabled(true);

    // 保存当前窗口信息用于下次比较
    m_lastWindowsInfo = currentWindowsInfo;
}

QList<QPair<HWND, MainWindow::WindowInfo>> MainWindow::getAllWindowsInfo() const
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

        // 获取窗口标题
        wchar_t title[256];
        GetWindowText(hwnd, title, 256);
        QString windowTitle = QString::fromWCharArray(title);

        // 过滤条件
        if (windowTitle.isEmpty() ||
            windowTitle == "Program Manager" ||
            !IsWindowVisible(hwnd)) {
            return TRUE;
        }

        // 获取进程名
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);

        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        wchar_t processName[MAX_PATH] = L"";
        QString processNameStr = "Unknown";

        if (process) {
            GetModuleFileNameEx(process, NULL, processName, MAX_PATH);
            processNameStr = QFileInfo(QString::fromWCharArray(processName)).fileName();
            CloseHandle(process);
        }

        WindowInfo info;
        info.title = windowTitle;
        info.processName = processNameStr;
        info.hwnd = hwnd;
        info.isVisible = IsWindowVisible(hwnd);
        info.isHidden = false; // 会在外部设置

        windowsList->append(qMakePair(hwnd, info));
        return TRUE;
        }, reinterpret_cast<LPARAM>(&windows));

    // 标记隐藏窗口
    for (auto& window : windows) {
        if (hiddenSet.contains(window.first)) {
            window.second.isHidden = true;
        }
    }

    return windows;
}
HWND MainWindow::getSelectedWindow() const
{
    int row = windowsTable->currentRow();
    if (row < 0) return nullptr;

    return reinterpret_cast<HWND>(windowsTable->item(row, 0)->data(Qt::UserRole).toULongLong());
}

void MainWindow::bringToFront()
{
    HWND hwnd = getSelectedWindow();
    if (hwnd) {
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
    }
}

void MainWindow::endTask()
{
    HWND hwnd = getSelectedWindow();
    if (hwnd) {
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);

        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
        if (process) {
            TerminateProcess(process, 0);
            CloseHandle(process);
            refreshAllLists();
            updateTrayMenu();
        }
    }
}

void MainWindow::refreshAllLists()
{
    refreshWindowsTable();
    refreshHiddenWindowsTable();
}

void MainWindow::loadLanguage(const QString& language)
{
    QString langFile = QString("./language/%1.lang").arg(language);
    if (!Translator::instance().loadLanguage(langFile)) {
        // 如果指定语言文件加载失败，尝试加载默认语言
        Translator::instance().loadLanguage("./language/zh.lang");
    }

    // 重新翻译所有UI文本
    retranslateUI();
}

void MainWindow::retranslateUI()
{
    // 更新窗口标题
    setWindowTitle(trc("MainWindow", "Traynex"));

    // 更新表格标题
    windowsTable->setHorizontalHeaderLabels({
        trc("MainWindow", "Window Title"),
        trc("MainWindow", "Process"),
        trc("MainWindow", "Handle")
        });
    hiddenWindowsTable->setHorizontalHeaderLabels({
        trc("MainWindow", "Window Title"),
        trc("MainWindow", "Process"),
        trc("MainWindow", "Handle")
        });

    // 更新标签页标题
    tabWidget->setTabText(0, trc("MainWindow", "Main"));
    tabWidget->setTabText(1, trc("MainWindow", "Hidden Windows"));
    tabWidget->setTabText(2, trc("MainWindow", "Settings"));
    tabWidget->setTabText(3, trc("MainWindow", "About"));

    // 更新托盘菜单
    if (trayIcon) {
        showAction->setText(trc("MainWindow", "Open Main Window"));
        restoreAllAction->setText(trc("MainWindow", "Restore All Windows"));
        quitAction->setText(trc("MainWindow", "Exit"));
        trayIcon->setToolTip(trc("MainWindow", "Traynex - Right click for menu"));
    }
    // 更新动态菜单布局
    updateTrayMenuLayout();
    // 更新设置页面
    // 组标题
    if (auto generalGroup = findChild<QGroupBox*>("generalGroup")) {
        generalGroup->setTitle(trc("MainWindow", "General Settings"));
    }
    if (auto refreshGroup = findChild<QGroupBox*>("refreshGroup")) {
        refreshGroup->setTitle(trc("MainWindow", "Auto Refresh Settings"));
    }
    if (auto windowGroup = findChild<QGroupBox*>("windowGroup")) {
        windowGroup->setTitle(trc("MainWindow", "Window Settings"));
    }

    // 复选框和标签
    startWithSystemCheck->setText(trc("MainWindow", "Start with Windows"));
    enableHotkeyCheck->setText(trc("MainWindow", "Enable Hotkey (Win+Shift+Z)"));
    autoRefreshCheck->setText(trc("MainWindow", "Enable auto refresh"));
    alwaysOnTopCheck->setText(trc("MainWindow", "Always on Top"));

    // 表单标签
    if (auto refreshLabel = findChild<QLabel*>("refreshIntervalLabel")) {
        refreshLabel->setText(trc("MainWindow", "Refresh interval:"));
    }
    if (auto maxWindowsLabel = findChild<QLabel*>("maxWindowsLabel")) {
        maxWindowsLabel->setText(trc("MainWindow", "Maximum hidden windows:"));
    }
    if (auto languageLabel = findChild<QLabel*>("languageLabel")) {
        languageLabel->setText(trc("MainWindow", "Language:"));
    }

    // 更新关于页面
    if (auto aboutTitle = findChild<QLabel*>("aboutTitle")) {
        aboutTitle->setText(trc("MainWindow", "About"));
    }

    // 关于页面按钮
    if (auto githubButton = findChild<QPushButton*>("githubButton")) {
        githubButton->setText(trc("MainWindow", "Visit GitHub Repository"));
    }
    if (auto checkUpdateButton = findChild<QPushButton*>("checkUpdateButton")) {
        checkUpdateButton->setText(trc("MainWindow", "Check for Updates"));
    }

    // 更新关于文本
    showAbout();

    // 更新右键菜单
    if (contextMenu) {
        hideToTrayAction->setText(trc("MainWindow", "Hide to Tray Icon"));
        hideToAppTrayAction->setText(trc("MainWindow", "Hide to Tray Menu"));
        bringToFrontAction->setText(trc("MainWindow", "Bring to Front"));
        highlightAction->setText(trc("MainWindow", "Highlight Window"));
        toggleOnTopAction->setText(trc("MainWindow", "Always on Top"));
        endTaskAction->setText(trc("MainWindow", "End Task"));
    }

    // 刷新表格内容
    refreshWindowsTable();
}

void MainWindow::onHotkeySettingChanged()
{
    bool enabled = enableHotkeyCheck->isChecked();

    // 立即应用热键设置
    WindowsTrayManager::instance().setHotkeyEnabled(enabled);

    qDebug() << "Hotkey setting changed:" << enabled;

    // 自动保存设置
    QTimer::singleShot(100, this, &MainWindow::autoSaveSettings);
}

void MainWindow::onRefreshSettingChanged()
{
    bool autoRefresh = autoRefreshCheck->isChecked();
    int interval = refreshIntervalSpin->value();

    // 立即应用刷新设置
    if (autoRefresh) {
        refreshTimer->start(interval);
    }
    else {
        refreshTimer->stop();
    }

    qDebug() << "Refresh setting changed - Auto:" << autoRefresh << "Interval:" << interval;

    // 自动保存设置
    QTimer::singleShot(100, this, &MainWindow::autoSaveSettings);
}

void MainWindow::onLanguageChanged()
{
    QString newLanguage = languageCombo->currentData().toString();
    QString oldLanguage = ""; // 可以从设置中获取旧值

    // 立即应用语言设置
    if (oldLanguage != newLanguage) {
        loadLanguage(newLanguage);
    }

    qDebug() << "Language changed to:" << newLanguage;

    // 自动保存设置
    QTimer::singleShot(100, this, &MainWindow::autoSaveSettings);
}

void MainWindow::onStartWithSystemChanged()
{
    bool startWithSystem = startWithSystemCheck->isChecked();

    // 设置开机自启动
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);

    QString appName = "Traynex";
    QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    if (startWithSystem) {
        settings.setValue(appName, appPath);
    }
    else {
        settings.remove(appName);
    }

    qDebug() << "Start with system changed:" << startWithSystem;

    // 自动保存设置
    QTimer::singleShot(100, this, &MainWindow::autoSaveSettings);
}

void MainWindow::onMaxWindowsChanged()
{
    int maxWindows = maxWindowsSpin->value();

    // 更新最大窗口限制
    // WindowsTrayManager::instance().setMaxWindows(maxWindows);

    qDebug() << "Max windows changed:" << maxWindows;

    // 自动保存设置
    QTimer::singleShot(100, this, &MainWindow::autoSaveSettings);
}

void MainWindow::autoSaveSettings()
{
    saveSettings();
    qDebug() << "Settings auto-saved";
}

void MainWindow::onAlwaysOnTopChanged()
{
    bool alwaysOnTop = alwaysOnTopCheck->isChecked();

    // 更新窗口标志
    updateWindowFlags();

    qDebug() << "Always on top changed:" << alwaysOnTop;

    // 自动保存设置
    QTimer::singleShot(100, this, &MainWindow::autoSaveSettings);
}

void MainWindow::updateWindowFlags()
{
    bool alwaysOnTop = alwaysOnTopCheck->isChecked();

    // 检查当前标志是否已经正确设置
    Qt::WindowFlags currentFlags = windowFlags();
    bool currentlyOnTop = currentFlags & Qt::WindowStaysOnTopHint;

    // 如果状态没有变化，不需要更新
    if (currentlyOnTop == alwaysOnTop) {
        return;
    }

    // 保存当前窗口状态
    bool wasVisible = isVisible();
    QPoint pos = this->pos();
    QSize size = this->size();
    int currentTab = tabWidget->currentIndex();

    // 设置新的窗口标志
    Qt::WindowFlags newFlags = currentFlags;

    if (alwaysOnTop) {
        newFlags |= Qt::WindowStaysOnTopHint;
    }
    else {
        newFlags &= ~Qt::WindowStaysOnTopHint;
    }

    // 重新设置窗口标志
    setWindowFlags(newFlags);

    // 恢复窗口状态
    move(pos);
    resize(size);
    tabWidget->setCurrentIndex(currentTab);

    // 如果窗口原本是可见的，重新显示
    if (wasVisible) {
        show();
        // 短暂延迟后再次置顶，确保效果
        QTimer::singleShot(10, this, [this]() {
            raise();
            activateWindow();
            });
    }

    qDebug() << "Window always on top:" << alwaysOnTop;
}

void MainWindow::highlightWindow()
{
    HWND hwnd = getSelectedWindow();
    if (!hwnd) {
        QMessageBox::information(this, trc("MainWindow", "Information"),
            trc("MainWindow", "Please select a window to highlight"));
        return;
    }

    if (!hwnd || !IsWindow(hwnd)) {
        QMessageBox::warning(this, trc("MainWindow", "Warning"),
            trc("MainWindow", "The selected window is no longer available"));
        refreshAllLists();
        return;
    }

    // 高亮选中的窗口
    flashWindowInTaskbar(hwnd);
}

void MainWindow::flashWindowInTaskbar(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    FlashWindow(hwnd, TRUE);

    qDebug() << "Window highlighted:" << QString::number(reinterpret_cast<qulonglong>(hwnd), 16);
}

bool MainWindow::isWindowOnTop(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }

    // 获取窗口扩展样式
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    return (style & WS_EX_TOPMOST) != 0;
}

void MainWindow::setWindowOnTop(HWND hwnd, bool onTop)
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

void MainWindow::toggleWindowOnTop()
{
    HWND hwnd = getSelectedWindow();
    if (!hwnd) {
        QMessageBox::information(this, trc("MainWindow", "Information"),
            trc("MainWindow", "Please select a window to toggle always on top"));
        return;
    }

    if (!hwnd || !IsWindow(hwnd)) {
        QMessageBox::warning(this, trc("MainWindow", "Warning"),
            trc("MainWindow", "The selected window is no longer available"));
        refreshAllLists();
        return;
    }

    // 检查当前状态并切换
    bool currentlyOnTop = isWindowOnTop(hwnd);
    setWindowOnTop(hwnd, !currentlyOnTop);

    // 刷新显示以更新状态
    refreshAllLists();
}

void MainWindow::refreshHiddenWindowsTable()
{
    hiddenWindowsTable->setSortingEnabled(false);
    hiddenWindowsTable->setRowCount(0);

    // 获取系统托盘隐藏的窗口
    auto systemHiddenWindows = WindowsTrayManager::instance().getHiddenWindows();

    // 获取应用托盘菜单隐藏的窗口
    auto appTrayHiddenWindows = m_appTrayWindows;

    // 合并两种隐藏窗口
    QMap<HWND, QString> allHiddenWindows;

    // 添加系统托盘隐藏窗口
    for (const auto& window : systemHiddenWindows) {
        HWND hwnd = window.first;
        if (hwnd && IsWindow(hwnd)) {
            allHiddenWindows[hwnd] = QString::fromStdWString(window.second);
        }
    }

    // 添加应用托盘菜单隐藏窗口
    for (auto it = appTrayHiddenWindows.begin(); it != appTrayHiddenWindows.end(); ++it) {
        HWND hwnd = it.key();
        if (hwnd && IsWindow(hwnd)) {
            // 如果窗口标题还没有获取，就获取一次
            if (!allHiddenWindows.contains(hwnd)) {
                wchar_t title[256];
                if (GetWindowText(hwnd, title, 256) > 0) {
                    allHiddenWindows[hwnd] = QString::fromWCharArray(title);
                }
                else {
                    allHiddenWindows[hwnd] = trc("MainWindow", "Unknown Window");
                }
            }
        }
    }

    // 显示所有隐藏窗口
    for (auto it = allHiddenWindows.begin(); it != allHiddenWindows.end(); ++it) {
        HWND hwnd = it.key();
        QString title = it.value();

        int row = hiddenWindowsTable->rowCount();
        hiddenWindowsTable->insertRow(row);

        // 窗口标题
        QTableWidgetItem* titleItem = new QTableWidgetItem(title);
        titleItem->setData(Qt::UserRole, reinterpret_cast<qulonglong>(hwnd));

        // 进程名
        QString processName = "Unknown";
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);

        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (process) {
            wchar_t processPath[MAX_PATH] = L"";
            if (GetModuleFileNameEx(process, NULL, processPath, MAX_PATH)) {
                processName = QFileInfo(QString::fromWCharArray(processPath)).fileName();
            }
            CloseHandle(process);
        }

        QTableWidgetItem* processItem = new QTableWidgetItem(processName);

        // 窗口句柄
        QTableWidgetItem* handleItem = new QTableWidgetItem(
            QString::number(reinterpret_cast<qulonglong>(hwnd), 16).toUpper());

        hiddenWindowsTable->setItem(row, 0, titleItem);
        hiddenWindowsTable->setItem(row, 1, processItem);
        hiddenWindowsTable->setItem(row, 2, handleItem);

        // 所有隐藏窗口都显示为灰色
        for (int col = 0; col < 3; ++col) {
            if (auto item = hiddenWindowsTable->item(row, col)) {
                item->setForeground(Qt::gray);
            }
        }
    }

    hiddenWindowsTable->setSortingEnabled(true);
}

void MainWindow::restoreSelectedHiddenWindow()
{
    int row = hiddenWindowsTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, trc("MainWindow", "Information"),
            trc("MainWindow", "Please select a window to restore"));
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(hiddenWindowsTable->item(row, 0)->data(Qt::UserRole).toULongLong());
    if (!hwnd || !IsWindow(hwnd)) {
        QMessageBox::warning(this, trc("MainWindow", "Warning"),
            trc("MainWindow", "The selected window is no longer available"));
        refreshAllLists();
        return;
    }

    bool success = false;

    // 尝试从系统托盘恢复
    success = WindowsTrayManager::instance().restoreWindow(hwnd);

    // 系统托盘恢复失败，尝试从应用托盘菜单恢复
    if (!success && m_appTrayWindows.contains(hwnd)) {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        removeWindowFromTrayMenu(hwnd);
        success = true;
    }

    if (success) {
        refreshAllLists();
        updateTrayMenu();
    }
    else {
        QMessageBox::warning(this, trc("MainWindow", "Error"),
            trc("MainWindow", "Failed to restore the window"));
    }
}

void MainWindow::onHiddenTableContextMenu(const QPoint& pos)
{
    if (!hiddenTableContextMenu) {
        hiddenTableContextMenu = new QMenu(this);

        restoreHiddenAction = new QAction(trc("MainWindow", "Restore Window"), this);
        restoreAllHiddenAction = new QAction(trc("MainWindow", "Restore All Windows"), this);

        hiddenTableContextMenu->addAction(restoreHiddenAction);
        hiddenTableContextMenu->addSeparator();
        hiddenTableContextMenu->addAction(restoreAllHiddenAction);

        // 一次性连接信号
        connect(restoreHiddenAction, &QAction::triggered, this, &MainWindow::restoreSelectedHiddenWindow);
        connect(restoreAllHiddenAction, &QAction::triggered, this, &MainWindow::restoreAllWindows);
    }

    // 获取选中的窗口
    int row = hiddenWindowsTable->rowAt(pos.y());
    HWND selectedHwnd = nullptr;

    if (row >= 0) {
        hiddenWindowsTable->setCurrentCell(row, 0);
        selectedHwnd = reinterpret_cast<HWND>(hiddenWindowsTable->item(row, 0)->data(Qt::UserRole).toULongLong());
    }

    // 根据状态更新菜单项
    restoreHiddenAction->setEnabled(selectedHwnd && IsWindow(selectedHwnd));

    // 检查所有类型的隐藏窗口
    auto systemHiddenWindows = WindowsTrayManager::instance().getHiddenWindows();
    bool hasSystemHiddenWindows = !systemHiddenWindows.empty();
    bool hasAppTrayHiddenWindows = !m_appTrayWindows.isEmpty();

    restoreAllHiddenAction->setEnabled(hasSystemHiddenWindows || hasAppTrayHiddenWindows);

    hiddenTableContextMenu->exec(hiddenWindowsTable->viewport()->mapToGlobal(pos));
}

void MainWindow::updateTrayMenu()
{
    // 安全检查
    if (!trayMenu || !restoreAllAction) {
        return;
    }

    // 更新菜单布局
    updateTrayMenuLayout();
}

void MainWindow::hideToAppTray()
{
    HWND hwnd = getSelectedWindow();
    if (!hwnd) {
        QMessageBox::information(this, trc("MainWindow", "Information"),
            trc("MainWindow", "Please select a window to hide"));
        return;
    }

    if (!hwnd || !IsWindow(hwnd)) {
        QMessageBox::warning(this, trc("MainWindow", "Warning"),
            trc("MainWindow", "The selected window is no longer available"));
        refreshAllLists();
        return;
    }

    // 检查是否是受保护的窗口
    wchar_t className[256];
    if (!GetClassName(hwnd, className, 256)) {
        QMessageBox::warning(this, trc("MainWindow", "Error"),
            trc("MainWindow", "Cannot get window class name"));
        return;
    }

    // 禁止隐藏系统关键窗口
    const wchar_t* restrictedWindows[] = {
        L"WorkerW",
        L"Shell_TrayWnd",
        L"Progman"
    };

    for (const wchar_t* restricted : restrictedWindows) {
        if (wcscmp(className, restricted) == 0) {
            QMessageBox::warning(this, trc("MainWindow", "Error"),
                trc("MainWindow", "Cannot hide system windows"));
            return;
        }
    }

    // 获取窗口标题
    wchar_t title[256];
    GetWindowText(hwnd, title, 256);
    QString windowTitle = QString::fromWCharArray(title);

    // 隐藏窗口
    ShowWindow(hwnd, SW_HIDE);

    // 添加到托盘菜单
    addWindowToTrayMenu(hwnd, windowTitle);

    // 刷新显示
    refreshAllLists();
    updateTrayMenu();

    QMessageBox::information(this, trc("MainWindow", "Success"),
        trc("MainWindow", "Window hidden to app tray successfully"));
}

void MainWindow::addWindowToTrayMenu(HWND hwnd, const QString& title)
{
    if (!trayMenu) {
        qWarning() << "trayMenu is null, cannot add window";
        return;
    }

    // 如果窗口已经存在，先移除
    if (m_appTrayWindows.contains(hwnd)) {
        QAction* oldAction = m_appTrayWindows[hwnd];
        if (oldAction) {
            trayMenu->removeAction(oldAction);
            oldAction->deleteLater();
        }
        m_appTrayWindows.remove(hwnd);
    }

    // 创建恢复该窗口的动作
    QString displayTitle = title;
    if (displayTitle.isEmpty()) {
        displayTitle = trc("MainWindow", "Unknown Window");
    }

    // 限制标题长度
    if (displayTitle.length() > 40) {
        displayTitle = displayTitle.left(37) + "...";
    }

    QAction* restoreAction = new QAction(displayTitle, trayMenu);
    restoreAction->setData(reinterpret_cast<qulonglong>(hwnd));

    connect(restoreAction, &QAction::triggered, this, &MainWindow::restoreWindowFromAppTray);

    // 添加到映射中
    m_appTrayWindows[hwnd] = restoreAction;

    // 更新菜单布局
    updateTrayMenuLayout();
}

void MainWindow::removeWindowFromTrayMenu(HWND hwnd)
{
    if (!trayMenu) {
        return;
    }

    if (m_appTrayWindows.contains(hwnd)) {
        QAction* action = m_appTrayWindows[hwnd];
        if (action) {
            trayMenu->removeAction(action);
            action->deleteLater();
        }
        m_appTrayWindows.remove(hwnd);
        updateTrayMenuLayout();
    }
}

void MainWindow::updateTrayMenuLayout()
{
    if (!trayMenu) {
        qWarning() << "trayMenu is null!";
        return;
    }

    // 清除现有的隐藏窗口动作
    QList<QAction*> actions = trayMenu->actions();

    // 找到分隔符的位置，保留固定动作
    int firstSeparatorIndex = -1;
    int secondSeparatorIndex = -1;

    for (int i = 0; i < actions.size(); ++i) {
        if (actions[i]->isSeparator()) {
            if (firstSeparatorIndex == -1) {
                firstSeparatorIndex = i;
            }
            else if (secondSeparatorIndex == -1) {
                secondSeparatorIndex = i;
                break;
            }
        }
    }

    // 移除第一个分隔符和第二个分隔符之间的所有动作
    if (firstSeparatorIndex != -1 && secondSeparatorIndex != -1) {
        for (int i = secondSeparatorIndex - 1; i > firstSeparatorIndex; --i) {
            QAction* action = actions[i];
            // 只移除不是固定动作的项
            if (action != showAction && action != restoreAllAction && action != quitAction && !action->isSeparator()) {
                trayMenu->removeAction(action);
            }
        }
    }

    // 清理无效的窗口
    QList<HWND> windowsToRemove;
    for (auto it = m_appTrayWindows.begin(); it != m_appTrayWindows.end(); ++it) {
        HWND hwnd = it.key();
        QAction* action = it.value();

        if (!hwnd || !IsWindow(hwnd)) {
            windowsToRemove.append(hwnd);
            if (action) {
                trayMenu->removeAction(action);
                action->deleteLater();
            }
        }
    }

    // 移除无效窗口
    for (HWND hwnd : windowsToRemove) {
        m_appTrayWindows.remove(hwnd);
    }

    // 如果有隐藏窗口，在第一个分隔符后添加它们
    if (!m_appTrayWindows.isEmpty()) {
        // 添加所有隐藏窗口
        for (auto it = m_appTrayWindows.begin(); it != m_appTrayWindows.end(); ++it) {
            HWND hwnd = it.key();
            QAction* action = it.value();

            if (hwnd && IsWindow(hwnd) && action) {
                trayMenu->insertAction(restoreAllAction, action);
            }
        }

        // 添加分隔符
        trayMenu->insertSeparator(restoreAllAction);
    }

    // 更新 restoreAllAction 状态
    auto systemHiddenWindows = WindowsTrayManager::instance().getHiddenWindows();
    restoreAllAction->setEnabled(!systemHiddenWindows.empty() || !m_appTrayWindows.isEmpty());
}

void MainWindow::restoreWindowFromAppTray()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(action->data().toULongLong());
    if (!hwnd || !IsWindow(hwnd)) {
        QMessageBox::warning(this, trc("MainWindow", "Warning"),
            trc("MainWindow", "The selected window is no longer available"));
        removeWindowFromTrayMenu(hwnd);
        refreshAllLists();
        return;
    }

    // 恢复窗口显示
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    // 从菜单中移除
    removeWindowFromTrayMenu(hwnd);

    // 刷新显示
    refreshAllLists();
    updateTrayMenu();

    qDebug() << "Window restored from app tray:" << QString::number(reinterpret_cast<qulonglong>(hwnd), 16);
}