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
    // 创建 UI
    setupUI();
    setupConnections();

    // 创建定时器
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::refreshWindowsTable);
    refreshTimer->start(500);

    // 加载设置
    loadSettings();

    QString language = languageCombo->currentData().toString();
    loadLanguage(language);

    setWindowTitle(trc("MainWindow", "My Application - Window Manager"));
    resize(600, 500);

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

    // 常规设置
    QGroupBox* generalGroup = new QGroupBox(trc("MainWindow", "General Settings"));
    generalGroup->setObjectName("generalGroup"); // 设置对象名称
    QVBoxLayout* generalLayout = new QVBoxLayout(generalGroup);

    startWithSystemCheck = new QCheckBox(trc("MainWindow", "Start with Windows"));
    enableHotkeyCheck = new QCheckBox(trc("MainWindow", "Enable Hotkey (Win+Shift+Z)"));
    enableHotkeyCheck->setChecked(true);

    generalLayout->addWidget(startWithSystemCheck);
    generalLayout->addWidget(enableHotkeyCheck);

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
    languageCombo->addItem("日本語", "ja");

    // 创建表单标签并设置对象名称
    QLabel* maxWindowsLabel = new QLabel(trc("MainWindow", "Maximum hidden windows:"));
    maxWindowsLabel->setObjectName("maxWindowsLabel");

    QLabel* languageLabel = new QLabel(trc("MainWindow", "Language:"));
    languageLabel->setObjectName("languageLabel");

    windowLayout->addRow(maxWindowsLabel, maxWindowsSpin);
    windowLayout->addRow(languageLabel, languageCombo);

    // 保存设置按钮
    saveSettingsButton = new QPushButton(trc("MainWindow", "Save Settings"));

    settingsLayout->addWidget(generalGroup);
    settingsLayout->addWidget(refreshGroup);
    settingsLayout->addWidget(windowGroup);
    settingsLayout->addWidget(saveSettingsButton);
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
        trc("MainWindow", "Window Tray Manager"),
        trc("MainWindow", "Version"),
        trc("MainWindow", "Description"),
        trc("MainWindow", "Hotkey"),
        trc("MainWindow", "Usage"),
        trc("MainWindow", "Thank you for using our application!")
    );

    aboutLabel->setText(aboutText);

    QPushButton* githubButton = new QPushButton(trc("MainWindow", "Visit GitHub Repository"));
    githubButton->setObjectName("githubButton"); // 设置对象名称

    QPushButton* checkUpdateButton = new QPushButton(trc("MainWindow", "Check for Updates"));
    checkUpdateButton->setObjectName("checkUpdateButton"); // 设置对象名称

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
        QDesktopServices::openUrl(QUrl("https://github.com/new5Fpointer/Traynex"));
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
    // 检查语言是否发生变化
    QString oldLanguage = ""; // 可以从设置中获取旧值
    QString newLanguage = languageCombo->currentData().toString();
    bool languageChanged = (oldLanguage != newLanguage);

    saveSettings();

    // 如果语言发生变化，重新加载语言
    if (languageChanged) {
        loadLanguage(newLanguage);
    }

    // 应用刷新设置
    if (autoRefreshCheck->isChecked()) {
        refreshTimer->start(refreshIntervalSpin->value());
    }
    else {
        refreshTimer->stop();
    }

    QMessageBox::information(this, trc("MainWindow", "Success"),
        trc("MainWindow", "Settings saved successfully!"));
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
            trc("MainWindow", "Window Tray Manager"),
            trc("MainWindow", "Version"),
            trc("MainWindow", "Description"),
            trc("MainWindow", "Hotkey"),
            trc("MainWindow", "Usage"),
            trc("MainWindow", "Thank you for using our application!")
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
    setWindowTitle(trc("MainWindow", "My Application - Window Manager"));

    // 更新表格标题
    windowsTable->setHorizontalHeaderLabels({
        trc("MainWindow", "Window Title"),
        trc("MainWindow", "Process"),
        trc("MainWindow", "Status"),
        trc("MainWindow", "Handle")
        });

    // 更新标签页标题
    tabWidget->setTabText(0, trc("MainWindow", "Main"));
    tabWidget->setTabText(1, trc("MainWindow", "Settings"));
    tabWidget->setTabText(2, trc("MainWindow", "About"));

    // 更新状态标签
    statusLabel->setText(trc("MainWindow", "Ready"));

    // 更新托盘菜单
    if (trayIcon) {
        showAction->setText(trc("MainWindow", "Open Main Window"));
        restoreAllAction->setText(trc("MainWindow", "Restore All Windows"));
        quitAction->setText(trc("MainWindow", "Exit"));
        trayIcon->setToolTip(trc("MainWindow", "Window Tray Manager - Right click for menu"));
    }

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

    // 按钮
    saveSettingsButton->setText(trc("MainWindow", "Save Settings"));

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
        hideToTrayAction->setText(trc("MainWindow", "Hide to Tray"));
        restoreAction->setText(trc("MainWindow", "Restore from Tray"));
        bringToFrontAction->setText(trc("MainWindow", "Bring to Front"));
        endTaskAction->setText(trc("MainWindow", "End Task"));
    }

    // 刷新状态标签
    refreshHiddenWindowsList();

    // 刷新表格内容
    refreshWindowsTable();
}