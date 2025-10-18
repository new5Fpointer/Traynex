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
#include <psapi.h>

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
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::refreshWindowsTable);
    refreshTimer->start(500); // 每500ms刷新一次

    // 初始隐藏主窗口
    hide();

    refreshAllLists();

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

    // === 主页面 - 任务管理器风格 ===
    QWidget* mainTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(mainTab);

    // 标题和工具栏
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->addStretch();

    // 创建表格
    windowsTable = new QTableWidget();
    windowsTable->setColumnCount(4);
    windowsTable->setHorizontalHeaderLabels({
        trc("MainWindow", "Window Title"),
        trc("MainWindow", "Process"),
        trc("MainWindow", "Status"),
        trc("MainWindow", "Handle")
        });

    // 表格属性
    windowsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    windowsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    windowsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    windowsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    windowsTable->setSortingEnabled(true);

    // 列宽设置
    windowsTable->setColumnWidth(0, 300); // 标题
    windowsTable->setColumnWidth(1, 150); // 进程名
    windowsTable->setColumnWidth(2, 100); // 状态
    windowsTable->setColumnWidth(3, 80);  // 句柄

    // 状态栏
    statusLabel = new QLabel(trc("MainWindow", "Ready"));
    statusLabel->setStyleSheet("color: gray; font-style: italic;");

    // 组装布局
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(windowsTable);
    mainLayout->addWidget(statusLabel);

    // 创建右键菜单
    createContextMenu();

    // 连接信号
    connect(windowsTable, &QTableWidget::customContextMenuRequested,
        this, &MainWindow::onTableContextMenu);

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
    refreshIntervalSpin->setRange(100, 1000);
    refreshIntervalSpin->setValue(500);
    refreshIntervalSpin->setSuffix(trc("MainWindow", "ms"));

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
    connect(githubButton, &QPushButton::clicked, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/your-repo/window-tray-manager"));
        });
    connect(checkUpdateButton, &QPushButton::clicked, this, &MainWindow::showAbout);
}

void MainWindow::setupConnections()
{
    connect(saveSettingsButton, &QPushButton::clicked, this, &MainWindow::updateSettings);
}

void MainWindow::refreshHiddenWindowsList()
{
    // 从 WindowsTrayManager 获取真实的隐藏窗口列表
    auto hiddenWindows = WindowsTrayManager::instance().getHiddenWindows();
    int hiddenCount = 0;

    for (const auto& window : hiddenWindows) {
        if (!window.first || !IsWindow(window.first)) {
            continue; // 跳过无效窗口
        }
        hiddenCount++;
    }

    // 更新状态标签
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

    // 调用 WindowsTrayManager 恢复单个窗口
    if (WindowsTrayManager::instance().restoreWindow(hwnd)) {
        // 成功恢复，刷新显示
        refreshAllLists();
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
        refreshTimer->start(refreshIntervalSpin->value());
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
    int refreshInterval = settings.value("refresh/interval", 500).toInt();
    refreshIntervalSpin->setValue(refreshInterval);

    // 应用刷新设置到定时器
    if (autoRefresh) {
        refreshTimer->start(refreshInterval);
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

    hideToTrayAction = new QAction(trc("MainWindow", "Hide to Tray"), this);
    restoreAction = new QAction(trc("MainWindow", "Restore from Tray"), this);
    bringToFrontAction = new QAction(trc("MainWindow", "Bring to Front"), this);
    endTaskAction = new QAction(trc("MainWindow", "End Task"), this);

    connect(hideToTrayAction, &QAction::triggered, this, &MainWindow::hideSelectedToTray);
    connect(restoreAction, &QAction::triggered, this, &MainWindow::restoreSelectedWindow);
    connect(bringToFrontAction, &QAction::triggered, this, &MainWindow::bringToFront);
    connect(endTaskAction, &QAction::triggered, this, &MainWindow::endTask);

    contextMenu->addAction(hideToTrayAction);
    contextMenu->addAction(restoreAction);
    contextMenu->addSeparator();
    contextMenu->addAction(bringToFrontAction);
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
    QTableWidgetItem* statusItem = windowsTable->item(row, 2);
    bool isHidden = statusItem && statusItem->text() == trc("MainWindow", "Hidden");

    hideToTrayAction->setEnabled(!isHidden);
    restoreAction->setEnabled(isHidden);
    bringToFrontAction->setEnabled(true);
    endTaskAction->setEnabled(true);

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

        // 状态
        QString status;
        QColor statusColor;
        if (window.second.isHidden) {
            status = trc("MainWindow", "Hidden");
            statusColor = Qt::red;
        }
        else if (window.second.isVisible) {
            status = trc("MainWindow", "Visible");
            statusColor = Qt::green;
        }
        else {
            status = trc("MainWindow", "Minimized");
            statusColor = Qt::blue;
        }

        QTableWidgetItem* statusItem = new QTableWidgetItem(status);
        statusItem->setForeground(statusColor);

        // 窗口句柄
        QTableWidgetItem* handleItem = new QTableWidgetItem(
            QString::number(reinterpret_cast<qulonglong>(window.second.hwnd), 16).toUpper());

        windowsTable->setItem(row, 0, titleItem);
        windowsTable->setItem(row, 1, processItem);
        windowsTable->setItem(row, 2, statusItem);
        windowsTable->setItem(row, 3, handleItem);

        // 隐藏窗口显示为灰色
        if (window.second.isHidden) {
            for (int col = 0; col < 4; ++col) {
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
        }
    }
}

void MainWindow::refreshAllLists()
{
    refreshWindowsTable();
    refreshHiddenWindowsList();
}
