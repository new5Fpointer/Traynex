#include "mainwindow.h"

#include "windowstraymanager.h"
#include "translator.h"
#include "hotkeymanager.h"
#include "volumecontrol.h"

#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QHeaderView>
#include <QFormLayout>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QThread>
#include <QWidgetAction>
#include <QProcess>
#include <QFileInfo>
#include <QClipboard>
#include <QMimeData>

#include <psapi.h>
#include <shellapi.h>
#include <QScrollArea>

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
	, restoreLastHiddenAction(nullptr)
	, restoreAllHiddenAction(nullptr)
	, hideToAppTrayAction(nullptr)
	, restoreLastAction(nullptr)
	, copyMenu(nullptr)
	, copyTitleAction(nullptr)
	, copyClassAction(nullptr)
	, copyPathAction(nullptr)
	, copyAllAction(nullptr)
{
	// 创建 UI
	setupUI();
	setupConnections();

	connect(&HotkeyManager::instance(), &HotkeyManager::hotkeyTriggered,
		this, &MainWindow::onHotkeyTriggered);
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

	initializeHotkeyTable();

	setWindowTitle("Traynex");
	resize(800, 600);

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
	connect(&HotkeyManager::instance(), &HotkeyManager::hotkeyTriggered,
		this, &MainWindow::onHotkeyTriggered);
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

	// 移除边框、网格线和行号
	windowsTable->setFrameShape(QFrame::NoFrame);
	windowsTable->setShowGrid(false);
	windowsTable->verticalHeader()->setVisible(false);

	// 设置默认的文本行为
	windowsTable->setTextElideMode(Qt::ElideRight);
	windowsTable->setProperty("wordWrap", false);

	// 表头设置
	windowsTable->setColumnCount(7);
	windowsTable->setHorizontalHeaderLabels({
		"",
		trc("MainWindow", "Window Title"),
		trc("MainWindow", "Handle"),
		trc("MainWindow", "Class"),
		trc("MainWindow", "Process ID"),
		trc("MainWindow", "Process"),
		trc("MainWindow", "Program Path")
		});
	
	// 为表头添加悬浮提示
	for (int i = 0; i < windowsTable->columnCount(); ++i) {
		QTableWidgetItem* headerItem = windowsTable->horizontalHeaderItem(i);
		if (headerItem) {
			headerItem->setToolTip(headerItem->text());
		}
	}
	
	windowsTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 标题左对齐
	windowsTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu); // 启用表头右键菜单

	// 表格属性
	windowsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	windowsTable->setSelectionMode(QAbstractItemView::SingleSelection);
	windowsTable->setContextMenuPolicy(Qt::CustomContextMenu);
	windowsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	windowsTable->setSortingEnabled(true);

	// QSS 样式
	windowsTable->setStyleSheet(
		"QTableWidget {"
		"    border: none;"
		"}"
		"QTableWidget::item:selected {"
		"    font-weight: normal;"
		"}"
	);

	// 列宽设置
	windowsTable->setColumnWidth(0, 24);  // 图标列
	windowsTable->setColumnWidth(1, 300); // 标题
	windowsTable->setColumnWidth(2, 80);  // 句柄
	windowsTable->setColumnWidth(3, 120); // 窗口类
	windowsTable->setColumnWidth(4, 80);  // 进程ID
	windowsTable->setColumnWidth(5, 200); // 进程名
	windowsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);    // 程序路径列拉伸
	windowsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);      // 图标列固定宽度
	windowsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
	windowsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
	windowsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
	windowsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
	windowsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
	
	// 默认隐藏程序路径列
	windowsTable->setColumnHidden(6, true);


	// 组装布局
	mainLayout->addLayout(headerLayout);
	mainLayout->addWidget(windowsTable);

	// 创建右键菜单
	createContextMenu();
	createHeaderContextMenu();

	// 连接信号
	connect(windowsTable, &QTableWidget::customContextMenuRequested,
		this, &MainWindow::onTableContextMenu);
	
	// 连接表头右键信号
	connect(windowsTable->horizontalHeader(), &QHeaderView::customContextMenuRequested,
		this, &MainWindow::onTableHeaderContextMenu);

	// === 隐藏窗口页面 ===
	QWidget* hiddenTab = new QWidget();
	QVBoxLayout* hiddenLayout = new QVBoxLayout(hiddenTab);

	// 创建隐藏窗口表格
	hiddenWindowsTable = new QTableWidget();

	// 移除边框、网格线和行号
	hiddenWindowsTable->setFrameShape(QFrame::NoFrame);
	hiddenWindowsTable->setShowGrid(false);
	hiddenWindowsTable->verticalHeader()->setVisible(false);

	// 设置默认的文本行为
	hiddenWindowsTable->setTextElideMode(Qt::ElideRight);
	hiddenWindowsTable->setProperty("wordWrap", false);

	// 表头设置
	hiddenWindowsTable->setColumnCount(7);
	hiddenWindowsTable->setHorizontalHeaderLabels({
		"",
		trc("MainWindow", "Window Title"),
		trc("MainWindow", "Handle"),
		trc("MainWindow", "Class"),
		trc("MainWindow", "Process ID"),
		trc("MainWindow", "Process"),
		trc("MainWindow", "Program Path")
		});
	
	// 为表头添加悬浮提示
	for (int i = 0; i < hiddenWindowsTable->columnCount(); ++i) {
		QTableWidgetItem* headerItem = hiddenWindowsTable->horizontalHeaderItem(i);
		if (headerItem) {
			headerItem->setToolTip(headerItem->text());
		}
	}
	
	hiddenWindowsTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 标题左对齐
	hiddenWindowsTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu); // 启用表头右键菜单

	// 表格属性
	hiddenWindowsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	hiddenWindowsTable->setSelectionMode(QAbstractItemView::SingleSelection);
	hiddenWindowsTable->setContextMenuPolicy(Qt::CustomContextMenu);
	hiddenWindowsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	hiddenWindowsTable->setSortingEnabled(true);

	// QSS 样式
	hiddenWindowsTable->setStyleSheet(
		"QTableWidget {"
		"    border: none;"
		"}"
		"QTableWidget::item:selected {"
		"    font-weight: normal;"
		"}"
	);

	// 列宽设置和拉伸
	hiddenWindowsTable->setColumnWidth(0, 24);  // 图标
	hiddenWindowsTable->setColumnWidth(1, 276); // 标题
	hiddenWindowsTable->setColumnWidth(2, 80);  // 句柄
	hiddenWindowsTable->setColumnWidth(3, 120); // 窗口类
	hiddenWindowsTable->setColumnWidth(4, 80);  // 进程ID
	hiddenWindowsTable->setColumnWidth(5, 200); // 进程名
	hiddenWindowsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);    // 程序路径列拉伸
	hiddenWindowsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);      // 图标列固定宽度
	hiddenWindowsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
	hiddenWindowsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
	hiddenWindowsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
	hiddenWindowsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
	hiddenWindowsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
	
	// 默认隐藏程序路径列
	hiddenWindowsTable->setColumnHidden(6, true);

	// 组装布局
	hiddenLayout->addWidget(hiddenWindowsTable);

	// 连接信号
	connect(hiddenWindowsTable, &QTableWidget::customContextMenuRequested,
		this, &MainWindow::onHiddenTableContextMenu);
	
	// 连接隐藏窗口表格表头右键信号
	connect(hiddenWindowsTable->horizontalHeader(), &QHeaderView::customContextMenuRequested,
		this, &MainWindow::onHiddenTableHeaderContextMenu);

	// === 设置页面 ===
	QWidget* settingsTab = new QWidget();
	QVBoxLayout* settingsLayout = new QVBoxLayout(settingsTab);
	settingsLayout->setContentsMargins(0, 0, 0, 0); // 移除边距

	// 创建一个滚动区域
	QScrollArea* scrollArea = new QScrollArea();
	scrollArea->setWidgetResizable(true); // 允许内部widget调整大小
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 禁用水平滚动条
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); // 根据需要显示垂直滚动条
	scrollArea->setFrameShape(QFrame::NoFrame); // 移除边框

	// 创建滚动区域的内容widget
	QWidget* scrollContent = new QWidget();
	QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
	scrollLayout->setAlignment(Qt::AlignTop); // 顶部对齐

	// 常规设置
	QGroupBox* generalGroup = new QGroupBox(trc("MainWindow", "General Settings"));
	generalGroup->setObjectName("generalGroup");
	QVBoxLayout* generalLayout = new QVBoxLayout(generalGroup);

	startWithSystemCheck = new QCheckBox(trc("MainWindow", "Start with Windows"));
	enableHotkeyCheck = new QCheckBox(trc("MainWindow", "Enable Hotkey"));
	enableHotkeyCheck->setChecked(true);

	alwaysOnTopCheck = new QCheckBox(trc("MainWindow", "Always on Top"));
	alwaysOnTopCheck->setToolTip(trc("MainWindow", "Keep the main window always on top of other windows"));

	generalLayout->addWidget(startWithSystemCheck);
	generalLayout->addWidget(enableHotkeyCheck);
	generalLayout->addWidget(alwaysOnTopCheck);

	// 自动刷新设置
	QGroupBox* refreshGroup = new QGroupBox(trc("MainWindow", "Auto Refresh Settings"));
	refreshGroup->setObjectName("refreshGroup");
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

	languageCombo = new QComboBox();
	languageCombo->addItem("English", "en");
	languageCombo->addItem("中文", "zh");

	QLabel* languageLabel = new QLabel(trc("MainWindow", "Language:"));
	languageLabel->setObjectName("languageLabel");

	windowLayout->addRow(languageLabel, languageCombo);

	// 热键设置
	QGroupBox* hotkeyGroup = new QGroupBox(trc("MainWindow", "Hotkey Settings"));
	hotkeyGroup->setObjectName("hotkeyGroup");
	hotkeyGroup->setFixedHeight(300);
	QVBoxLayout* hotkeyLayout = new QVBoxLayout(hotkeyGroup);

	// 创建热键表格
	hotkeyTable = new QTableWidget();
	hotkeyTable->setColumnCount(3);
	hotkeyTable->setHorizontalHeaderLabels({
		trc("MainWindow", "Action"),
		trc("MainWindow", "Description"),
		trc("MainWindow", "Hotkey")
		});
	
	// 为表头添加悬浮提示
	for (int i = 0; i < hotkeyTable->columnCount(); ++i) {
		QTableWidgetItem* headerItem = hotkeyTable->horizontalHeaderItem(i);
		if (headerItem) {
			headerItem->setToolTip(headerItem->text());
		}
	}

	// 表格属性
	hotkeyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	hotkeyTable->setSelectionMode(QAbstractItemView::SingleSelection);
	hotkeyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	hotkeyTable->verticalHeader()->setVisible(false);
	hotkeyTable->setShowGrid(false);
	hotkeyTable->setAlternatingRowColors(true);
	hotkeyTable->setTextElideMode(Qt::ElideRight);
	hotkeyTable->setProperty("wordWrap", false);

	// 列宽设置
	hotkeyTable->setColumnWidth(0, 120);  // 动作ID
	hotkeyTable->setColumnWidth(1, 200);  // 描述
	hotkeyTable->setColumnWidth(2, 150);  // 热键
	hotkeyTable->horizontalHeader()->setStretchLastSection(true);

	// 按钮布局
	QHBoxLayout* buttonLayout = new QHBoxLayout();
	bindHotkeyButton = new QPushButton(trc("MainWindow", "Bind Hotkey"));
	clearHotkeyButton = new QPushButton(trc("MainWindow", "Clear Hotkey"));

	bindHotkeyButton->setEnabled(false);
	clearHotkeyButton->setEnabled(false);

	buttonLayout->addWidget(bindHotkeyButton);
	buttonLayout->addWidget(clearHotkeyButton);
	buttonLayout->addStretch();

	// 组装热键布局
	hotkeyLayout->addWidget(hotkeyTable);
	hotkeyLayout->addLayout(buttonLayout);

	// 组装滚动区域的内容布局
	scrollLayout->addWidget(generalGroup);
	scrollLayout->addWidget(refreshGroup);
	scrollLayout->addWidget(windowGroup);
	scrollLayout->addWidget(hotkeyGroup);

	// 添加重置按钮
	QHBoxLayout* resetLayout = new QHBoxLayout();

	resetDefaultsButton = new QPushButton(trc("MainWindow", "Reset to Defaults"));
	resetDefaultsButton->setObjectName("resetDefaultsButton");
	resetDefaultsButton->setMaximumWidth(150);

	connect(resetDefaultsButton, &QPushButton::clicked, this, &MainWindow::onResetDefaults);

	resetLayout->addStretch();
	resetLayout->addWidget(resetDefaultsButton);
	resetLayout->addStretch();

	scrollLayout->addLayout(resetLayout);

	// 设置滚动区域的内容widget
	scrollArea->setWidget(scrollContent);

	// 确保滚动区域背景透明，与主窗口一致
	scrollArea->setStyleSheet(R"(
    QScrollArea {
        border: none;
        background: transparent;
    }
    QScrollArea > QWidget > QWidget {
        background: transparent;
    }
    QScrollBar:vertical {
        border: none;
        background: #f0f0f0;
        width: 8px;
        margin: 0px 0px 0px 0px;
    }
    QScrollBar::handle:vertical {
        background: #c0c0c0;
        border-radius: 4px;
        min-height: 20px;
    }
    QScrollBar::handle:vertical:hover {
        background: #a0a0a0;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
    }
)");

	// 设置滚动区域背景与主窗口一致
	QPalette palette = scrollArea->palette();
	palette.setColor(QPalette::Window, QApplication::palette().color(QPalette::Window));
	scrollArea->setPalette(palette);
	scrollContent->setPalette(palette);

	// 组装主布局
	settingsLayout->addWidget(scrollArea);

	// 添加标签页
	tabWidget->addTab(settingsTab, trc("MainWindow", "Settings"));
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
	githubButton->setStyleSheet("QPushButton { padding: 8px; font-weight: bold; }");

	aboutLayout->addWidget(aboutLabel);
	aboutLayout->addSpacing(20);
	aboutLayout->addWidget(githubButton);
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
}

void MainWindow::setupConnections()
{
	connect(autoRefreshCheck, &QCheckBox::stateChanged, this, &MainWindow::onRefreshSettingChanged);
	connect(refreshIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onRefreshSettingChanged);
	connect(languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onLanguageChanged);
	connect(startWithSystemCheck, &QCheckBox::stateChanged, this, &MainWindow::onStartWithSystemChanged);
	connect(alwaysOnTopCheck, &QCheckBox::stateChanged, this, &MainWindow::onAlwaysOnTopChanged);
	connect(bindHotkeyButton, &QPushButton::clicked, this, &MainWindow::startBindHotkey);
	connect(clearHotkeyButton, &QPushButton::clicked, this, &MainWindow::clearSelectedHotkey);
	connect(hotkeyTable, &QTableWidget::itemDoubleClicked, this, &MainWindow::onHotkeyItemDoubleClicked);
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

	m_hiddenWindowOrder.clear();

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
			m_hiddenWindowOrder.removeAll(foregroundWindow);
			m_hiddenWindowOrder.prepend(foregroundWindow);

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

	restoreLastAction = new QAction(trc("MainWindow", "Restore Last Window"), this);
	connect(restoreLastAction, &QAction::triggered, this, &MainWindow::restoreLastWindow);

	restoreAllAction = new QAction(trc("MainWindow", "Restore All Windows"), this);
	connect(restoreAllAction, &QAction::triggered, this, &MainWindow::restoreAllWindows);

	quitAction = new QAction(trc("MainWindow", "Exit"), this);
	connect(quitAction, &QAction::triggered, this, &MainWindow::closeApp);

	trayMenu->addAction(showAction);
	trayMenu->addSeparator();
	trayMenu->addSeparator();
	trayMenu->addAction(restoreLastAction);
	trayMenu->addAction(restoreAllAction);
	trayMenu->addSeparator();
	trayMenu->addAction(quitAction);

	// 创建托盘图标
	trayIcon = new QSystemTrayIcon(this);

	// 设置图标
	QIcon icon(":/icon/icon.png");
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
	QString configPath = getConfigPath();

	// 创建默认配置
	if (!QFile::exists(configPath)) {
		createDefaultConfig();
	}

	QSettings settings(configPath, QSettings::IniFormat);

	// 热键设置
	bool hotkeyEnabled = settings.value("hotkey/enabled", true).toBool();
	enableHotkeyCheck->setChecked(hotkeyEnabled);

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

	loadHotkeySettings();

	// 应用加载的设置
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
	settings.setValue("window/always_on_top", alwaysOnTopCheck->isChecked());

	// 常规设置
	settings.setValue("general/start_with_system", startWithSystemCheck->isChecked());
	settings.setValue("general/language", languageCombo->currentData().toString());

	// 刷新设置
	settings.setValue("refresh/auto_refresh", autoRefreshCheck->isChecked());
	settings.setValue("refresh/interval", refreshIntervalSpin->value());

	settings.sync(); // 立即写入磁盘
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
		// 记录隐藏顺序
		m_hiddenWindowOrder.removeAll(hwnd);  // 先移除（如果已存在）
		m_hiddenWindowOrder.prepend(hwnd);    // 添加到开头（最近隐藏的）

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
	muteAction = new QAction(trc("MainWindow", "Mute Process"), this);
	opacityMenu = new QMenu(trc("MainWindow", "Opacity"), contextMenu);
	opacitySlider = new QSlider(Qt::Horizontal);
	opacityLabel = new QLabel;
	openFolderAction = new QAction(trc("MainWindow", "Open File Location"), this);
	filePropsAction = new QAction(trc("MainWindow", "File Properties"), this);
	endTaskAction = new QAction(trc("MainWindow", "End Task"), this);
	
	// 复制功能
	copyMenu = new QMenu(trc("MainWindow", "Copy"), contextMenu);
	copyTitleAction = new QAction(trc("MainWindow", "Copy Title"), this);
	copyClassAction = new QAction(trc("MainWindow", "Copy Class"), this);
	copyPathAction = new QAction(trc("MainWindow", "Copy Path"), this);
	copyAllAction = new QAction(trc("MainWindow", "Copy All"), this);

	toggleOnTopAction->setCheckable(true);
	muteAction->setCheckable(true);

	opacitySlider->setRange(10, 100);
	opacitySlider->setValue(20);
	opacityLabel->setText("100%");

	connect(hideToTrayAction, &QAction::triggered, this, &MainWindow::hideSelectedToTray);
	connect(hideToAppTrayAction, &QAction::triggered, this, &MainWindow::hideToAppTray);
	connect(bringToFrontAction, &QAction::triggered, this, &MainWindow::bringToFront);
	connect(highlightAction, &QAction::triggered, this, &MainWindow::highlightWindow);
	connect(toggleOnTopAction, &QAction::triggered, this, &MainWindow::toggleWindowOnTop);
	connect(muteAction, &QAction::triggered, this, &MainWindow::toggleMuteWindow);
	connect(opacitySlider, &QSlider::valueChanged, this, &MainWindow::onOpacitySliderChanged);
	connect(openFolderAction, &QAction::triggered, this, &MainWindow::openFileLocation);
	connect(filePropsAction, &QAction::triggered, this, &MainWindow::showFileProperties);
	connect(endTaskAction, &QAction::triggered, this, &MainWindow::endTask);
	
	// 连接复制功能
	connect(copyTitleAction, &QAction::triggered, this, &MainWindow::copyTitle);
	connect(copyClassAction, &QAction::triggered, this, &MainWindow::copyClass);
	connect(copyPathAction, &QAction::triggered, this, &MainWindow::copyPath);
	connect(copyAllAction, &QAction::triggered, this, &MainWindow::copyAll);

	auto* sliderAction = new QWidgetAction(opacityMenu);
	auto* sliderWidget = new QWidget;
	auto* hLay = new QHBoxLayout(sliderWidget);
	hLay->addWidget(opacitySlider, 1);
	hLay->addWidget(opacityLabel);
	hLay->setContentsMargins(6, 2, 6, 2);
	sliderAction->setDefaultWidget(sliderWidget);
	opacityMenu->addAction(sliderAction);

	contextMenu->addAction(hideToTrayAction);
	contextMenu->addAction(hideToAppTrayAction);
	contextMenu->addSeparator();
	contextMenu->addAction(bringToFrontAction);
	contextMenu->addAction(highlightAction);
	contextMenu->addAction(toggleOnTopAction);
	contextMenu->addAction(muteAction);
	contextMenu->addMenu(opacityMenu);
	contextMenu->addSeparator();
	
	// 添加复制菜单
	copyMenu->addAction(copyTitleAction);
	copyMenu->addAction(copyClassAction);
	copyMenu->addAction(copyPathAction);
	copyMenu->addAction(copyAllAction);
	contextMenu->addMenu(copyMenu);
	
	contextMenu->addSeparator();
	contextMenu->addAction(openFolderAction);
	contextMenu->addAction(filePropsAction);
	contextMenu->addSeparator();
	contextMenu->addAction(endTaskAction);
}

void MainWindow::createHeaderContextMenu()
{
	headerContextMenu = new QMenu(this);

	// 创建列显示/隐藏动作
	showHandleColumnAction = new QAction(trc("MainWindow", "Handle"), this);
	showClassColumnAction = new QAction(trc("MainWindow", "Class"), this);
	showPidColumnAction = new QAction(trc("MainWindow", "Process ID"), this);
	showProcessColumnAction = new QAction(trc("MainWindow", "Process"), this);
	showProgramPathColumnAction = new QAction(trc("MainWindow", "Program Path"), this);
	resetColumnWidthsAction = new QAction(trc("MainWindow", "Reset Column Widths"), this);

	// 设置所有动作为可勾选
	showHandleColumnAction->setCheckable(true);
	showClassColumnAction->setCheckable(true);
	showPidColumnAction->setCheckable(true);
	showProcessColumnAction->setCheckable(true);
	showProgramPathColumnAction->setCheckable(true);

	// 默认所有列都显示（程序路径列默认隐藏）
	showHandleColumnAction->setChecked(true);
	showClassColumnAction->setChecked(true);
	showPidColumnAction->setChecked(true);
	showProcessColumnAction->setChecked(true);
	showProgramPathColumnAction->setChecked(false);

	// 连接信号
	connect(showHandleColumnAction, &QAction::triggered, [this]() { toggleColumnVisibility(2); });
	connect(showClassColumnAction, &QAction::triggered, [this]() { toggleColumnVisibility(3); });
	connect(showPidColumnAction, &QAction::triggered, [this]() { toggleColumnVisibility(4); });
	connect(showProcessColumnAction, &QAction::triggered, [this]() { toggleColumnVisibility(5); });
	connect(showProgramPathColumnAction, &QAction::triggered, [this]() { toggleColumnVisibility(6); });
	connect(resetColumnWidthsAction, &QAction::triggered, [this]() { 
		resetTableColumnWidths(windowsTable);
		resetTableColumnWidths(hiddenWindowsTable);
	});

	// 添加动作到菜单
	headerContextMenu->addAction(showHandleColumnAction);
	headerContextMenu->addAction(showClassColumnAction);
	headerContextMenu->addAction(showPidColumnAction);
	headerContextMenu->addAction(showProcessColumnAction);
	headerContextMenu->addAction(showProgramPathColumnAction);
	headerContextMenu->addSeparator();
	headerContextMenu->addAction(resetColumnWidthsAction);
}

void MainWindow::onTableHeaderContextMenu(const QPoint& pos)
{
	// 更新菜单项状态
	showHandleColumnAction->setChecked(!windowsTable->isColumnHidden(2));
	showClassColumnAction->setChecked(!windowsTable->isColumnHidden(3));
	showPidColumnAction->setChecked(!windowsTable->isColumnHidden(4));
	showProcessColumnAction->setChecked(!windowsTable->isColumnHidden(5));
	showProgramPathColumnAction->setChecked(!windowsTable->isColumnHidden(6));

	// 显示菜单
	headerContextMenu->exec(windowsTable->horizontalHeader()->viewport()->mapToGlobal(pos));
}

void MainWindow::onHiddenTableHeaderContextMenu(const QPoint& pos)
{
	// 更新菜单项状态
	showHandleColumnAction->setChecked(!hiddenWindowsTable->isColumnHidden(2));
	showClassColumnAction->setChecked(!hiddenWindowsTable->isColumnHidden(3));
	showPidColumnAction->setChecked(!hiddenWindowsTable->isColumnHidden(4));
	showProcessColumnAction->setChecked(!hiddenWindowsTable->isColumnHidden(5));
	showProgramPathColumnAction->setChecked(!hiddenWindowsTable->isColumnHidden(6));

	// 显示菜单
	headerContextMenu->exec(hiddenWindowsTable->horizontalHeader()->viewport()->mapToGlobal(pos));
}

void MainWindow::toggleColumnVisibility(int column)
{
	// 禁止隐藏图标列(0)和窗口标题列(1)
	if (column == 0 || column == 1) {
		return;
	}
	
	// 切换主表格列的显示/隐藏
	bool isHidden = windowsTable->isColumnHidden(column);
	windowsTable->setColumnHidden(column, !isHidden);
	
	// 切换隐藏窗口表格列的显示/隐藏
	hiddenWindowsTable->setColumnHidden(column, !isHidden);
	
	// 更新最后一列的拉伸模式
	updateLastColumnStretchMode();
}

void MainWindow::resetTableColumnWidths(QTableWidget* table)
{
	if (!table) return;

	// 恢复默认列宽
	table->setColumnWidth(0, 24);   // 图标列
	table->setColumnWidth(1, 300);  // 标题
	table->setColumnWidth(2, 80);   // 句柄
	table->setColumnWidth(3, 120);  // 窗口类
	table->setColumnWidth(4, 80);   // 进程ID
	table->setColumnWidth(5, 200);  // 进程名
	
	// 设置列宽模式
	table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);      // 图标列固定
	table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
	table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
	table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
	table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
	table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
	table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);    // 程序路径列拉伸
	
	// 更新拉伸模式
	updateLastColumnStretchMode();
}

void MainWindow::updateLastColumnStretchMode()
{
	// 查找最后一个可见列
	int lastVisibleColumn = -1;
	for (int i = windowsTable->columnCount() - 1; i >= 0; --i) {
		if (!windowsTable->isColumnHidden(i)) {
			lastVisibleColumn = i;
			break;
		}
	}
	
	if (lastVisibleColumn >= 0) {
		// 重置所有列的拉伸模式
		for (int i = 0; i < windowsTable->columnCount(); ++i) {
			if (i == 0) {
				windowsTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed); // 图标列固定
			} else if (i == lastVisibleColumn) {
				windowsTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch); // 最后一列拉伸
			} else {
				windowsTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
			}
		}
		
		// 对隐藏窗口表格应用相同的设置
		for (int i = 0; i < hiddenWindowsTable->columnCount(); ++i) {
			if (i == 0) {
				hiddenWindowsTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed); // 图标列固定
			} else if (i == lastVisibleColumn) {
				hiddenWindowsTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch); // 最后一列拉伸
			} else {
				hiddenWindowsTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
			}
		}
	}
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

	DWORD processId = 0;
	GetWindowThreadProcessId(hwnd, &processId);
	bool isMuted = muteStates.value(processId, false);
	muteAction->setChecked(isMuted);

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

	if (row >= 0) {
		HWND hwnd = reinterpret_cast<HWND>(windowsTable->item(row, 0)->data(Qt::UserRole).toULongLong());
		BYTE curAlpha = 255;
		if (hwnd && IsWindow(hwnd))
			GetLayeredWindowAttributes(hwnd, nullptr, &curAlpha, nullptr);
		opacitySlider->setValue(curAlpha * 0.390625 + 1);
	}

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
				currentWindowsInfo[i].second.processName != m_lastWindowsInfo[i].second.processName ||
				currentWindowsInfo[i].second.className != m_lastWindowsInfo[i].second.className ||
				currentWindowsInfo[i].second.processId != m_lastWindowsInfo[i].second.processId) {
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

	// 设置列数为7，添加图标列和程序路径列
	windowsTable->setColumnCount(7);
	windowsTable->setHorizontalHeaderLabels({
		"", // 图标列
		trc("MainWindow", "Window Title"),
		trc("MainWindow", "Handle"),
		trc("MainWindow", "Class"),
		trc("MainWindow", "Process ID"),
		trc("MainWindow", "Process"),
		trc("MainWindow", "Program Path")
		});

	// 设置列提示
	for (int i = 0; i < windowsTable->columnCount(); ++i) {
		QTableWidgetItem* headerItem = windowsTable->horizontalHeaderItem(i);
		if (headerItem && !headerItem->text().isEmpty()) {
			headerItem->setToolTip(headerItem->text());
		}
	}

	for (const auto& window : currentWindowsInfo) {
		int row = windowsTable->rowCount();
		windowsTable->insertRow(row);

		// 图标
		QTableWidgetItem* iconItem = new QTableWidgetItem();
		if (!window.second.icon.isNull()) {
			iconItem->setIcon(window.second.icon);
		}
		iconItem->setData(Qt::UserRole, reinterpret_cast<qulonglong>(window.second.hwnd));

		// 窗口标题
		QTableWidgetItem* titleItem = new QTableWidgetItem(window.second.title);
		titleItem->setData(Qt::UserRole, reinterpret_cast<qulonglong>(window.second.hwnd));
		titleItem->setToolTip(window.second.title);  // 添加悬浮提示

		// 窗口句柄
		QTableWidgetItem* handleItem = new QTableWidgetItem(
			QString::number(reinterpret_cast<qulonglong>(window.second.hwnd), 16).toUpper());
		handleItem->setToolTip(QString::number(reinterpret_cast<qulonglong>(window.second.hwnd), 16).toUpper());

		// 窗口类名
		QTableWidgetItem* classItem = new QTableWidgetItem(window.second.className);
		classItem->setToolTip(window.second.className);

		// 进程ID
		QTableWidgetItem* pidItem = new QTableWidgetItem(QString::number(window.second.processId));
		pidItem->setToolTip(QString::number(window.second.processId));

		// 进程名
		QTableWidgetItem* processItem = new QTableWidgetItem(window.second.processName);
		processItem->setToolTip(window.second.processName);

		// 程序路径
		QTableWidgetItem* pathItem = new QTableWidgetItem(window.second.processPath);
		pathItem->setToolTip(window.second.processPath);

		windowsTable->setItem(row, 0, iconItem);     // 图标
		windowsTable->setItem(row, 1, titleItem);    // 窗口标题
		windowsTable->setItem(row, 2, handleItem);   // 句柄
		windowsTable->setItem(row, 3, classItem);    // 类
		windowsTable->setItem(row, 4, pidItem);      // 进程ID
		windowsTable->setItem(row, 5, processItem);  // 进程名
		windowsTable->setItem(row, 6, pathItem);     // 程序路径

		// 隐藏窗口显示为灰色
		if (window.second.isHidden) {
			for (int col = 0; col < 7; ++col) {
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

	// 调整列宽
	windowsTable->setColumnWidth(0, 24);  // 图标列宽度
	windowsTable->setColumnWidth(1, 276); // 标题（调整后）
	windowsTable->setColumnWidth(2, 80);  // 句柄
	windowsTable->setColumnWidth(3, 120); // 窗口类
	windowsTable->setColumnWidth(4, 80);  // 进程ID
	windowsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

	windowsTable->setSortingEnabled(true);

	// 保存当前窗口信息用于下次比较
	m_lastWindowsInfo = currentWindowsInfo;
}

QList<QPair<HWND, MainWindow::WindowInfo>> MainWindow::getAllWindowsInfo() const
{
	QList<QPair<HWND, WindowInfo>> windows;
	DWORD currentProcessId = GetCurrentProcessId();

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

		// 使用getWindowInfo获取窗口信息（注意：这里需要调用MainWindow的getWindowInfo）
		// 由于在lambda中无法直接调用成员函数，我们暂时复制getWindowInfo的逻辑
		WindowInfo info;
		info.hwnd = hwnd;
		
		// 获取窗口标题
		wchar_t title[256];
		GetWindowText(hwnd, title, 256);
		info.originalTitle = QString::fromWCharArray(title);
		info.title = info.originalTitle;
		
		// 过滤零宽空格和其他不可见控制字符
		info.title.remove(QChar(0x200B));  // 零宽空格
		info.title.remove(QChar(0x200C));  // 零宽非连接符
		info.title.remove(QChar(0x200D));  // 零宽连接符
		info.title.remove(QChar(0xFEFF));  // 零宽无中断空格
		
		info.className = windowClass;
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
		
		info.isVisible = IsWindowVisible(hwnd);
		info.isHidden = false; // 会在外部设置
		
		// 获取窗口图标
		QIcon windowIcon;
		HICON hIcon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0);
		if (!hIcon) {
			hIcon = (HICON)GetClassLongPtr(hwnd, GCLP_HICONSM);
		}
		if (!hIcon) {
			hIcon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0);
		}
		if (!hIcon) {
			hIcon = (HICON)GetClassLongPtr(hwnd, GCLP_HICON);
		}

		// 如果仍然没有图标，尝试从进程获取
		if (!hIcon && processId) {
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
			if (hProcess) {
				wchar_t exePath[MAX_PATH];
				if (GetModuleFileNameEx(hProcess, NULL, exePath, MAX_PATH)) {
					HICON hAppIcon = ExtractIcon(GetModuleHandle(NULL), exePath, 0);
					if (hAppIcon) {
						hIcon = hAppIcon;
					}
				}
				CloseHandle(hProcess);
			}
		}

		// 如果获取到图标，转换为QIcon
		if (hIcon) {
			windowIcon = QIcon(QPixmap::fromImage(QImage::fromHICON(hIcon)));

			// 清理系统图标资源（如果是我们自己提取的）
			if (hIcon != (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0) &&
				hIcon != (HICON)GetClassLongPtr(hwnd, GCLP_HICONSM) &&
				hIcon != (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0) &&
				hIcon != (HICON)GetClassLongPtr(hwnd, GCLP_HICON)) {
				DestroyIcon(hIcon);
			}
		}
		info.icon = windowIcon;

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

MainWindow::WindowInfo MainWindow::getWindowInfo(HWND hwnd, bool filterInvisibleChars) const
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
		// 使用getWindowInfo获取进程ID
		WindowInfo info = getWindowInfo(hwnd);
		DWORD processId = info.processId;

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
	QString appDir = QCoreApplication::applicationDirPath();
	QString langFile = QString("%1/language/%2.lang").arg(appDir).arg(language);
	if (!Translator::instance().loadLanguage(langFile)) {
		// 如果指定语言文件加载失败，尝试加载默认语言
		QString defaultLangFile = QString("%1/language/zh.lang").arg(appDir);
		if (!Translator::instance().loadLanguage(defaultLangFile)) {
			qWarning() << "Failed to load default language file:" << defaultLangFile;
		}
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
		"", // 图标列
		trc("MainWindow", "Window Title"),
		trc("MainWindow", "Handle"),
		trc("MainWindow", "Class"),
		trc("MainWindow", "Process ID"),
		trc("MainWindow", "Process"),
		trc("MainWindow", "Program Path")
		});

	hiddenWindowsTable->setHorizontalHeaderLabels({
		"", // 图标列
		trc("MainWindow", "Window Title"),
		trc("MainWindow", "Handle"),
		trc("MainWindow", "Class"),
		trc("MainWindow", "Process ID"),
		trc("MainWindow", "Process"),
		trc("MainWindow", "Program Path")
		});

	// 更新表格列提示
	for (int i = 0; i < windowsTable->columnCount(); ++i) {
		QTableWidgetItem* headerItem = windowsTable->horizontalHeaderItem(i);
		if (headerItem && !headerItem->text().isEmpty()) {
			headerItem->setToolTip(headerItem->text());
		}
	}
	
	for (int i = 0; i < hiddenWindowsTable->columnCount(); ++i) {
		QTableWidgetItem* headerItem = hiddenWindowsTable->horizontalHeaderItem(i);
		if (headerItem && !headerItem->text().isEmpty()) {
			headerItem->setToolTip(headerItem->text());
		}
	}

	// 标签页标题
	tabWidget->setTabText(0, trc("MainWindow", "Main"));
	tabWidget->setTabText(1, trc("MainWindow", "Hidden Windows"));
	tabWidget->setTabText(2, trc("MainWindow", "Settings"));
	tabWidget->setTabText(3, trc("MainWindow", "About"));

	// 托盘菜单
	if (trayIcon) {
		showAction->setText(trc("MainWindow", "Open Main Window"));
		restoreLastAction->setText(trc("MainWindow", "Restore Last Window"));
		restoreAllAction->setText(trc("MainWindow", "Restore All Windows"));
		quitAction->setText(trc("MainWindow", "Exit"));
		trayIcon->setToolTip(trc("MainWindow", "Traynex - Right click for menu"));
	}

	// 更新托盘菜单布局
	updateTrayMenuLayout();

	// 设置页面组标题
	QList<QGroupBox*> groups = findChildren<QGroupBox*>();
	for (QGroupBox* group : groups) {
		QString objectName = group->objectName();
		if (objectName == "generalGroup") {
			group->setTitle(trc("MainWindow", "General Settings"));
		}
		else if (objectName == "refreshGroup") {
			group->setTitle(trc("MainWindow", "Auto Refresh Settings"));
		}
		else if (objectName == "windowGroup") {
			group->setTitle(trc("MainWindow", "Window Settings"));
		}
		else if (objectName == "hotkeyGroup") {
			group->setTitle(trc("MainWindow", "Hotkey Settings"));
		}
	}

	startWithSystemCheck->setText(trc("MainWindow", "Start with Windows"));
	enableHotkeyCheck->setText(trc("MainWindow", "Enable Hotkey"));
	autoRefreshCheck->setText(trc("MainWindow", "Enable auto refresh"));
	alwaysOnTopCheck->setText(trc("MainWindow", "Always on Top"));

	// 刷新间隔标签
	if (auto refreshLabel = findChild<QLabel*>("refreshIntervalLabel")) {
		refreshLabel->setText(trc("MainWindow", "Refresh interval:"));
	}

	// 语言标签
	if (auto languageLabel = findChild<QLabel*>("languageLabel")) {
		languageLabel->setText(trc("MainWindow", "Language:"));
	}

	// 热键表格标题
	hotkeyTable->setHorizontalHeaderLabels({
		trc("MainWindow", "Action"),
		trc("MainWindow", "Description"),
		trc("MainWindow", "Hotkey")
		});

	// 按钮文本
	bindHotkeyButton->setText(trc("MainWindow", "Bind Hotkey"));
	clearHotkeyButton->setText(trc("MainWindow", "Clear Hotkey"));

	// 重置按钮文本
	resetDefaultsButton->setText(trc("MainWindow", "Reset to Defaults"));

	// 刷新热键列表的描述
	initializeHotkeyTable();

	// 右键菜单
	if (contextMenu) {
		hideToTrayAction->setText(trc("MainWindow", "Hide to Tray Icon"));
		hideToAppTrayAction->setText(trc("MainWindow", "Hide to Tray Menu"));
		bringToFrontAction->setText(trc("MainWindow", "Bring to Front"));
		highlightAction->setText(trc("MainWindow", "Highlight Window"));
		toggleOnTopAction->setText(trc("MainWindow", "Always on Top"));
		muteAction->setText(trc("MainWindow", "Mute Process"));
		opacityMenu->setTitle(trc("MainWindow", "Opacity"));
		openFolderAction->setText(trc("MainWindow", "Open File Location"));
		filePropsAction->setText(trc("MainWindow", "File Properties"));
		endTaskAction->setText(trc("MainWindow", "End Task"));
		
		// 复制菜单
		if (copyMenu) {
			copyMenu->setTitle(trc("MainWindow", "Copy"));
			copyTitleAction->setText(trc("MainWindow", "Copy Title"));
			copyClassAction->setText(trc("MainWindow", "Copy Class"));
			copyPathAction->setText(trc("MainWindow", "Copy Path"));
			copyAllAction->setText(trc("MainWindow", "Copy All"));
		}
	}

	// 隐藏窗口表格右键菜单
	if (hiddenTableContextMenu) {
		restoreHiddenAction->setText(trc("MainWindow", "Restore Window"));
		restoreLastHiddenAction->setText(trc("MainWindow", "Restore Last Window"));
		restoreAllHiddenAction->setText(trc("MainWindow", "Restore All Windows"));
	}

	// 表头右键菜单
	if (headerContextMenu) {
		showHandleColumnAction->setText(trc("MainWindow", "Handle"));
		showClassColumnAction->setText(trc("MainWindow", "Class"));
		showPidColumnAction->setText(trc("MainWindow", "Process ID"));
		showProcessColumnAction->setText(trc("MainWindow", "Process"));
		showProgramPathColumnAction->setText(trc("MainWindow", "Program Path"));
		resetColumnWidthsAction->setText(trc("MainWindow", "Reset Column Widths"));
	}

	// 更新关于文本
	showAbout();

	// 刷新表格内容
	refreshWindowsTable();
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

void MainWindow::autoSaveSettings()
{
	saveSettings();
	saveHotkeySettings();
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
	QMap<HWND, std::tuple<QString, QString, QString, DWORD, QIcon>> allHiddenWindows;

	// 添加系统托盘隐藏窗口
	for (const auto& window : systemHiddenWindows) {
		HWND hwnd = window.first;
		if (hwnd && IsWindow(hwnd)) {
			// 使用getWindowInfo获取窗口信息
			WindowInfo info = getWindowInfo(hwnd);
			
			allHiddenWindows[hwnd] = std::make_tuple(
				QString::fromStdWString(window.second),
				info.processName,
				info.className,
				info.processId,
				info.icon
			);
		}
	}

	// 添加应用托盘菜单隐藏窗口
	for (auto it = appTrayHiddenWindows.begin(); it != appTrayHiddenWindows.end(); ++it) {
		HWND hwnd = it.key();
		if (hwnd && IsWindow(hwnd)) {
			if (!allHiddenWindows.contains(hwnd)) {
				// 使用getWindowInfo获取窗口信息
				WindowInfo info = getWindowInfo(hwnd);
				
				allHiddenWindows[hwnd] = std::make_tuple(info.title, info.processName, info.className, info.processId, info.icon);
			}
		}
	}

	// 显示所有隐藏窗口
	for (auto it = allHiddenWindows.begin(); it != allHiddenWindows.end(); ++it) {
		HWND hwnd = it.key();
		auto [title, processName, className, processId, icon] = it.value();

		int row = hiddenWindowsTable->rowCount();
		hiddenWindowsTable->insertRow(row);

		// 图标
		QTableWidgetItem* iconItem = new QTableWidgetItem();
		if (!icon.isNull()) {
			iconItem->setIcon(icon);
		}
		iconItem->setData(Qt::UserRole, reinterpret_cast<qulonglong>(hwnd));

		// 窗口标题
		QTableWidgetItem* titleItem = new QTableWidgetItem(title);
		titleItem->setData(Qt::UserRole, reinterpret_cast<qulonglong>(hwnd));
		titleItem->setToolTip(title);  // 添加悬浮提示

		// 窗口句柄
		QTableWidgetItem* handleItem = new QTableWidgetItem(
			QString::number(reinterpret_cast<qulonglong>(hwnd), 16).toUpper());
		handleItem->setToolTip(QString::number(reinterpret_cast<qulonglong>(hwnd), 16).toUpper());

		// 窗口类名
		QTableWidgetItem* classItem = new QTableWidgetItem(className);
		classItem->setToolTip(className);

		// 进程ID
		QTableWidgetItem* pidItem = new QTableWidgetItem(QString::number(processId));
		pidItem->setToolTip(QString::number(processId));

		// 进程名
		QTableWidgetItem* processItem = new QTableWidgetItem(processName);
		processItem->setToolTip(processName);

		hiddenWindowsTable->setItem(row, 0, iconItem);     // 图标
		hiddenWindowsTable->setItem(row, 1, titleItem);    // 窗口标题
		hiddenWindowsTable->setItem(row, 2, handleItem);   // 句柄
		hiddenWindowsTable->setItem(row, 3, classItem);    // 类
		hiddenWindowsTable->setItem(row, 4, pidItem);      // 进程ID
		hiddenWindowsTable->setItem(row, 5, processItem);  // 进程名
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
		m_hiddenWindowOrder.removeAll(hwnd);
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
		restoreLastHiddenAction = new QAction(trc("MainWindow", "Restore Last Window"), this);
		restoreAllHiddenAction = new QAction(trc("MainWindow", "Restore All Windows"), this);

		hiddenTableContextMenu->addAction(restoreHiddenAction);
		hiddenTableContextMenu->addAction(restoreLastHiddenAction);
		hiddenTableContextMenu->addSeparator();
		hiddenTableContextMenu->addAction(restoreAllHiddenAction);

		connect(restoreHiddenAction, &QAction::triggered, this, &MainWindow::restoreSelectedHiddenWindow);
		connect(restoreLastHiddenAction, &QAction::triggered, this, &MainWindow::restoreLastWindow);
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
	restoreLastHiddenAction->setEnabled(!m_hiddenWindowOrder.isEmpty());

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

	restoreLastAction->setEnabled(!m_hiddenWindowOrder.isEmpty());

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

	// 使用getWindowInfo获取窗口信息
	WindowInfo info = getWindowInfo(hwnd);
	
	// 禁止隐藏系统关键窗口
	const wchar_t* restrictedWindows[] = {
		L"WorkerW",
		L"Shell_TrayWnd",
		L"Progman"
	};

	for (const wchar_t* restricted : restrictedWindows) {
		if (info.className.compare(QString::fromWCharArray(restricted), Qt::CaseInsensitive) == 0) {
			QMessageBox::warning(this, trc("MainWindow", "Error"),
				trc("MainWindow", "Cannot hide system windows"));
			return;
		}
	}

	// 使用从getWindowInfo获取的标题和图标
	QString windowTitle = info.title;
	QIcon windowIcon = info.icon;

	// 隐藏窗口
	ShowWindow(hwnd, SW_HIDE);

	// 记录隐藏顺序
	m_hiddenWindowOrder.removeAll(hwnd);  // 先移除
	m_hiddenWindowOrder.prepend(hwnd);    // 添加到开头

	// 添加到托盘菜单（带图标）
	addWindowToTrayMenu(hwnd, windowTitle, windowIcon);

	// 刷新显示
	refreshAllLists();
	updateTrayMenu();

	QMessageBox::information(this, trc("MainWindow", "Success"),
		trc("MainWindow", "Window hidden to app tray successfully"));
}

void MainWindow::addWindowToTrayMenu(HWND hwnd, const QString& title, const QIcon& icon)
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

	// 获取或创建图标
	QIcon windowIcon = icon;
	if (windowIcon.isNull()) {
		windowIcon = getWindowIcon(hwnd);
	}

	// 如果仍然没有图标，使用默认图标
	if (windowIcon.isNull()) {
		windowIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
	}

	// 创建显示标题
	QString displayTitle = title;
	if (displayTitle.isEmpty()) {
		displayTitle = trc("MainWindow", "Unknown Window");
	}

	// 限制标题长度
	if (displayTitle.length() > 40) {
		displayTitle = displayTitle.left(37) + "...";
	}

	// 创建恢复该窗口的动作
	QAction* restoreAction = new QAction(windowIcon, displayTitle, trayMenu);

	// 使用 QVariantMap 存储完整窗口信息
	QVariantMap windowData;
	windowData["hwnd"] = reinterpret_cast<qulonglong>(hwnd);
	windowData["title"] = title;
	windowData["icon"] = windowIcon;

	// 获取进程信息用于显示
	DWORD processId;
	GetWindowThreadProcessId(hwnd, &processId);
	QString processName = "Unknown";

	HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
	if (process) {
		wchar_t processPath[MAX_PATH] = L"";
		if (GetModuleFileNameEx(process, NULL, processPath, MAX_PATH)) {
			processName = QFileInfo(QString::fromWCharArray(processPath)).fileName();
		}
		CloseHandle(process);
	}
	windowData["processName"] = processName;

	restoreAction->setData(windowData);

	// 设置工具提示显示更详细的信息
	QString toolTip = QString("%1\nProcess: %2\nHandle: 0x%3")
		.arg(title)
		.arg(processName)
		.arg(QString::number(reinterpret_cast<qulonglong>(hwnd), 16).toUpper());
	restoreAction->setToolTip(toolTip);

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
			if (action != showAction &&
				action != restoreLastAction &&
				action != restoreAllAction &&
				action != quitAction &&
				!action->isSeparator()) {
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
		QList<QAction*> actions = trayMenu->actions();
		int targetSeparatorIndex = -1;
		int separatorCount = 0;

		for (int i = 0; i < actions.size(); ++i) {
			if (actions[i]->isSeparator()) {
				separatorCount++;
				if (separatorCount == 2) { // 第二个分隔符
					targetSeparatorIndex = i;
					break;
				}
			}
		}

		if (targetSeparatorIndex != -1) {
			// 在第二个分隔符之前添加隐藏窗口
			for (auto it = m_appTrayWindows.begin(); it != m_appTrayWindows.end(); ++it) {
				HWND hwnd = it.key();
				QAction* action = it.value();

				if (hwnd && IsWindow(hwnd) && action) {
					trayMenu->insertAction(actions[targetSeparatorIndex], action);
				}
			}
		}
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

	// 从 QVariantMap 中获取窗口信息
	QVariantMap windowData = action->data().toMap();
	HWND hwnd = reinterpret_cast<HWND>(windowData["hwnd"].toULongLong());
	QString title = windowData["title"].toString();

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

	m_hiddenWindowOrder.removeAll(hwnd);

	// 刷新显示
	refreshAllLists();
	updateTrayMenu();

	qDebug() << "Window restored from app tray:" << title
		<< "Handle:" << QString::number(reinterpret_cast<qulonglong>(hwnd), 16);
}

void MainWindow::restoreLastWindow()
{
	if (m_hiddenWindowOrder.isEmpty()) {
		QMessageBox::information(this, trc("MainWindow", "Information"),
			trc("MainWindow", "No hidden windows to restore"));
		return;
	}

	// 获取最近隐藏的窗口
	HWND lastHwnd = m_hiddenWindowOrder.takeFirst();  // 从列表中移除

	if (!lastHwnd || !IsWindow(lastHwnd)) {
		// 如果窗口无效，递归尝试下一个
		m_hiddenWindowOrder.removeAll(lastHwnd);
		restoreLastWindow();
		return;
	}

	bool success = false;

	// 尝试从系统托盘恢复
	success = WindowsTrayManager::instance().restoreWindow(lastHwnd);

	// 系统托盘恢复失败，尝试从应用托盘菜单恢复
	if (!success && m_appTrayWindows.contains(lastHwnd)) {
		ShowWindow(lastHwnd, SW_SHOW);
		SetForegroundWindow(lastHwnd);
		removeWindowFromTrayMenu(lastHwnd);
		success = true;
	}

	if (success) {
		refreshAllLists();
		updateTrayMenu();
	}
	else {
		// 恢复失败，将窗口重新放回列表开头
		m_hiddenWindowOrder.prepend(lastHwnd);
		QMessageBox::warning(this, trc("MainWindow", "Error"),
			trc("MainWindow", "Failed to restore the last window"));
	}
}

QIcon MainWindow::getWindowIcon(HWND hwnd) const
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

void MainWindow::updateTrayMenuIcons()
{
	// 更新所有托盘菜单项的图标
	for (auto it = m_appTrayWindows.begin(); it != m_appTrayWindows.end(); ++it) {
		HWND hwnd = it.key();
		QAction* action = it.value();

		if (hwnd && IsWindow(hwnd) && action) {
			// 检查当前图标是否有效
			if (action->icon().isNull()) {
				QIcon newIcon = getWindowIcon(hwnd);
				if (!newIcon.isNull()) {
					action->setIcon(newIcon);

					// 更新存储的数据
					QVariantMap windowData = action->data().toMap();
					windowData["icon"] = newIcon;
					action->setData(windowData);
				}
			}
		}
	}
}

void MainWindow::loadHotkeySettings()
{
	QSettings settings(getConfigPath(), QSettings::IniFormat);

	settings.beginGroup("Hotkeys");

	QStringList keys = settings.childKeys();
	for (const QString& key : keys) {
		QString hotkeyStr = settings.value(key).toString();
		if (!hotkeyStr.isEmpty()) {
			QKeySequence keySequence(hotkeyStr);
			if (!HotkeyManager::instance().registerHotkey(key, keySequence)) {
				qWarning() << "Failed to register hotkey:" << key << "->" << hotkeyStr;
			}
		}
	}

	settings.endGroup();
}

void MainWindow::saveHotkeySettings()
{
	QSettings settings(getConfigPath(), QSettings::IniFormat);

	settings.beginGroup("Hotkeys");

	// 保存所有预定义热键动作的键绑定
	QVector<QPair<QString, QString>> hotkeyActions = {
		{"minimize_active", ""},
		{"show_window", ""},
		{"restore_last", ""},
		{"restore_all", ""}
	};

	auto currentHotkeys = HotkeyManager::instance().getAllHotkeys();

	for (const auto& action : hotkeyActions) {
		if (currentHotkeys.contains(action.first)) {
			settings.setValue(action.first, currentHotkeys[action.first].toString());
		}
		else {
			settings.setValue(action.first, "");  // 清空未注册的热键
		}
	}

	settings.endGroup();
	settings.sync();

	qDebug() << "Hotkey settings saved";
}

void MainWindow::onHotkeyTriggered(const QString& id)
{
	if (id == "minimize_active") {
		minimizeActiveToTray();
	}
	else if (id == "show_window") {
		showWindow();
	}
	else if (id == "restore_last") {
		restoreLastWindow();
	}
	else if (id == "restore_all") {
		restoreAllWindows();
	}
	else {
		qDebug() << "Unknown hotkey triggered:" << id;
	}
}

// 事件过滤器来捕获按键
bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
	if (m_settingHotkey && event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

		// 忽略单个修饰键
		if (keyEvent->key() == Qt::Key_Control ||
			keyEvent->key() == Qt::Key_Shift ||
			keyEvent->key() == Qt::Key_Alt ||
			keyEvent->key() == Qt::Key_Meta) {
			return QMainWindow::eventFilter(obj, event);
		}

		// 检查是否按下了 Escape 取消
		if (keyEvent->key() == Qt::Key_Escape) {
			cancelHotkeySetting();
			return true;
		}

		// 创建键序列
		QKeySequence keySequence(keyEvent->key() | keyEvent->modifiers());

		// 检查热键是否已被占用
		if (!isHotkeyAvailable(keySequence)) {
			cancelHotkeySetting();
			return true;
		}

		// 尝试注册热键
		if (HotkeyManager::instance().registerHotkey(m_currentHotkeyId, keySequence)) {
			finishHotkeySetting(keySequence.toString());
		}
		else {
			QMessageBox::warning(this, trc("MainWindow", "Error"),
				trc("MainWindow", "Failed to register hotkey"));
			cancelHotkeySetting();
		}

		return true;
	}

	return QMainWindow::eventFilter(obj, event);
}

void MainWindow::finishHotkeySetting(const QString& keySequence)
{
	m_settingHotkey = false;

	// 移除事件过滤器
	qApp->removeEventFilter(this);

	// 恢复UI状态
	hotkeyTable->setEnabled(true);
	bindHotkeyButton->setEnabled(true);
	clearHotkeyButton->setEnabled(true);

	// 更新表格显示
	int row = hotkeyTable->currentRow();
	if (row >= 0) {
		QTableWidgetItem* hotkeyItem = hotkeyTable->item(row, 2);
		if (hotkeyItem) {
			hotkeyItem->setText(keySequence);
			hotkeyItem->setForeground(Qt::black);
		}
	}

	// 保存设置
	saveHotkeySettings();

	QMessageBox::information(this, trc("MainWindow", "Success"),
		trc("MainWindow", "Hotkey set successfully"));

	// 清理临时对象
	if (currentHotkeyAction) {
		currentHotkeyAction->deleteLater();
		currentHotkeyAction = nullptr;
	}
}

void MainWindow::cancelHotkeySetting()
{
	if (!m_settingHotkey) return;

	m_settingHotkey = false;
	m_currentHotkeyId.clear();

	// 移除事件过滤器
	qApp->removeEventFilter(this);

	// 恢复UI状态
	hotkeyTable->setEnabled(true);
	bindHotkeyButton->setEnabled(true);
	clearHotkeyButton->setEnabled(true);

	// 恢复表格显示
	int row = hotkeyTable->currentRow();
	if (row >= 0) {
		// 获取原来的热键
		QString originalHotkey = "";
		QTableWidgetItem* idItem = hotkeyTable->item(row, 0);
		if (idItem) {
			QString hotkeyId = idItem->data(Qt::UserRole).toString();
			auto hotkeys = HotkeyManager::instance().getAllHotkeys();
			if (hotkeys.contains(hotkeyId)) {
				originalHotkey = hotkeys[hotkeyId].toString();
			}
		}

		QTableWidgetItem* hotkeyItem = hotkeyTable->item(row, 2);
		if (hotkeyItem) {
			hotkeyItem->setText(originalHotkey);
			hotkeyItem->setForeground(Qt::black);
		}
	}

	// 清理临时对象
	if (currentHotkeyAction) {
		currentHotkeyAction->deleteLater();
		currentHotkeyAction = nullptr;
	}
}

void MainWindow::onOpacitySliderChanged(int val)
{
	int row = windowsTable->currentRow();
	if (row < 0) return;

	HWND hwnd = reinterpret_cast<HWND>(windowsTable->item(row, 0)->data(Qt::UserRole).toULongLong());
	if (!hwnd || !IsWindow(hwnd)) return;

	BYTE alpha = static_cast<BYTE>(val / 0.390625 - 1);
	opacityLabel->setText(QString("%1%").arg(val));

	LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	if (!(exStyle & WS_EX_LAYERED))
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

	SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
}

void MainWindow::toggleMuteWindow()
{
	HWND hwnd = getSelectedWindow();
	if (!hwnd) {
		QMessageBox::information(this, trc("MainWindow", "Information"),
			trc("MainWindow", "Please select a window to mute/unmute"));
		return;
	}

	// 使用getWindowInfo获取进程ID
	WindowInfo info = getWindowInfo(hwnd);
	DWORD processId = info.processId;

	bool current = muteStates.value(processId, false);
	bool success = VolumeControl::SetProcessMuteWithTimeout(processId, !current, 1000);

	if (success) {
		muteStates[processId] = !current;
		QMessageBox::information(this, trc("MainWindow", "Success"),
			trc("MainWindow", "Window %1.").arg(current ? "unmuted" : "muted"));
	}
	else {
		QMessageBox::warning(this, trc("MainWindow", "Error"),
			trc("MainWindow", "Failed to mute/unmute process."));
	}
}

void MainWindow::openFileLocation()
{
	HWND hwnd = getSelectedWindow();
	if (!hwnd) return;

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (!pid) return;

	HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!hProc) return;
	wchar_t path[MAX_PATH]{};
	DWORD len = MAX_PATH;
	QueryFullProcessImageNameW(hProc, 0, path, &len);
	CloseHandle(hProc);
	if (!len) return;

	QString fullPath = QDir::toNativeSeparators(QString::fromWCharArray(path));
	if (!QFileInfo::exists(fullPath)) return;

	QString args = "/select,\"" + fullPath + "\"";

	SHELLEXECUTEINFO sei{};
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.lpFile = L"explorer.exe";
	sei.nShow = SW_SHOW;
	sei.fMask = SEE_MASK_INVOKEIDLIST;
	sei.lpParameters = reinterpret_cast<LPCWSTR>(args.utf16());
	ShellExecuteExW(&sei);
}

void MainWindow::showFileProperties()
{
	HWND hwnd = getSelectedWindow();
	if (!hwnd) return;

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (!pid) return;

	HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!hProc) return;
	wchar_t path[MAX_PATH]{};
	DWORD len = MAX_PATH;
	QueryFullProcessImageNameW(hProc, 0, path, &len);
	CloseHandle(hProc);
	if (!len) return;

	if (!QFileInfo::exists(QString::fromWCharArray(path))) return;

	SHELLEXECUTEINFO sei{};
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.lpFile = path;
	sei.nShow = SW_SHOW;
	sei.fMask = SEE_MASK_INVOKEIDLIST;
	sei.lpVerb = L"properties";
	ShellExecuteExW(&sei);
}

void MainWindow::createDefaultConfig()
{
	QSettings settings(getConfigPath(), QSettings::IniFormat);

	// 通用
	settings.setValue("general/language", "zh");
	settings.setValue("general/start_with_system", false);

	// 热键
	settings.setValue("hotkey/enabled", true);

	// 设置默认热键
	settings.beginGroup("Hotkeys");
	settings.setValue("minimize_active", "Win+Shift+Z");
	settings.setValue("show_window", "Win+Shift+X");
	settings.setValue("restore_last", "Win+Shift+R");
	settings.setValue("restore_all", "Win+Shift+A");
	settings.endGroup();

	// 窗口
	settings.setValue("window/always_on_top", false);

	// 刷新
	settings.setValue("refresh/auto_refresh", true);
	settings.setValue("refresh/interval", 500);

	settings.sync();
}

void MainWindow::onResetDefaults()
{
	int ret = QMessageBox::question(this,
		trc("MainWindow", "Confirm Reset"),
		trc("MainWindow", "Are you sure you want to reset all settings to default?"));

	if (ret != QMessageBox::Yes)
		return;

	createDefaultConfig();      // 写入默认配置
	loadSettings();             // 重新加载界面设置
	retranslateUI();            // 刷新语言

	QMessageBox::information(this,
		trc("MainWindow", "Reset Complete"),
		trc("MainWindow", "All settings have been restored to default."));
}

void MainWindow::initializeHotkeyTable()
{
	// 清空表格
	hotkeyTable->setRowCount(0);

	// 定义可用的热键动作
	QVector<QPair<QString, QString>> hotkeyActions = {
		{"minimize_active", trc("MainWindow", "Minimize Active Window to Tray")},
		{"show_window", trc("MainWindow", "Show Main Window")},
		{"restore_last", trc("MainWindow", "Restore Last Hidden Window")},
		{"restore_all", trc("MainWindow", "Restore All Hidden Windows")}
	};

	// 获取当前已注册的热键
	auto currentHotkeys = HotkeyManager::instance().getAllHotkeys();

	// 填充表格
	for (int i = 0; i < hotkeyActions.size(); ++i) {
		const auto& action = hotkeyActions[i];

		int row = hotkeyTable->rowCount();
		hotkeyTable->insertRow(row);

		// 动作ID
		QTableWidgetItem* idItem = new QTableWidgetItem(action.first);
		idItem->setData(Qt::UserRole, action.first);  // 保存ID
		idItem->setToolTip(action.first);  // 添加悬浮提示

		// 描述
		QTableWidgetItem* descItem = new QTableWidgetItem(action.second);
		descItem->setToolTip(action.second);  // 添加悬浮提示

		// 热键
		QString hotkeyText = "";
		if (currentHotkeys.contains(action.first)) {
			hotkeyText = currentHotkeys[action.first].toString();
		}
		QTableWidgetItem* hotkeyItem = new QTableWidgetItem(hotkeyText);
		hotkeyItem->setToolTip(hotkeyText);  // 添加悬浮提示

		hotkeyTable->setItem(row, 0, idItem);
		hotkeyTable->setItem(row, 1, descItem);
		hotkeyTable->setItem(row, 2, hotkeyItem);

		// 隐藏ID列
		hotkeyTable->setColumnHidden(0, true);
	}

	// 连接表格选择变化信号
	connect(hotkeyTable, &QTableWidget::itemSelectionChanged,
		this, &MainWindow::onHotkeySelectionChanged);
}

void MainWindow::onHotkeySelectionChanged()
{
	int row = hotkeyTable->currentRow();
	bool hasSelection = (row >= 0);

	bindHotkeyButton->setEnabled(hasSelection);
	clearHotkeyButton->setEnabled(hasSelection);
}

void MainWindow::startBindHotkey()
{
	int row = hotkeyTable->currentRow();
	if (row < 0) return;

	QTableWidgetItem* idItem = hotkeyTable->item(row, 0);
	if (!idItem) return;

	QString hotkeyId = idItem->data(Qt::UserRole).toString();
	currentHotkeyAction = new QAction(this);  // 临时存储当前设置的热键

	m_settingHotkey = true;
	m_currentHotkeyId = hotkeyId;

	// 改变UI状态提示用户
	QTableWidgetItem* hotkeyItem = hotkeyTable->item(row, 2);
	if (hotkeyItem) {
		hotkeyItem->setText(trc("MainWindow", "Press key combination..."));
		hotkeyItem->setForeground(Qt::blue);
	}

	bindHotkeyButton->setEnabled(false);
	clearHotkeyButton->setEnabled(false);
	hotkeyTable->setEnabled(false);

	// 安装事件过滤器
	qApp->installEventFilter(this);

	// 设置超时取消
	QTimer::singleShot(10000, this, [this]() {
		if (m_settingHotkey) {
			cancelHotkeySetting();
		}
		});
}

bool MainWindow::isHotkeyAvailable(const QKeySequence& keySequence)
{
	auto currentHotkeys = HotkeyManager::instance().getAllHotkeys();

	// 检查新热键是否与已有的冲突
	for (auto it = currentHotkeys.begin(); it != currentHotkeys.end(); ++it) {
		if (it.key() != m_currentHotkeyId && it.value() == keySequence) {
			return false;
		}
	}

	if (HotkeyManager::isSystemReservedHotkey(keySequence)) {
		QMessageBox::warning(this, trc("MainWindow", "Warning"),
			trc("MainWindow", "This hotkey is reserved by the system! Please choose another combination."));
		return false;
	}
	return true;
}

void MainWindow::clearSelectedHotkey()
{
	int row = hotkeyTable->currentRow();
	if (row < 0) return;

	QTableWidgetItem* idItem = hotkeyTable->item(row, 0);
	if (!idItem) return;

	QString hotkeyId = idItem->data(Qt::UserRole).toString();

	// 询问确认
	QMessageBox::StandardButton reply;
	reply = QMessageBox::question(this, trc("MainWindow", "Confirm"),
		trc("MainWindow", "Clear this hotkey?"),
		QMessageBox::Yes | QMessageBox::No);

	if (reply != QMessageBox::Yes) return;

	// 注销热键
	HotkeyManager::instance().unregisterHotkey(hotkeyId);

	// 更新表格
	QTableWidgetItem* hotkeyItem = hotkeyTable->item(row, 2);
	if (hotkeyItem) {
		hotkeyItem->setText("");
	}

	// 保存设置
	saveHotkeySettings();
}

void MainWindow::onHotkeyItemDoubleClicked(QTableWidgetItem* item)
{
	if (item && item->column() == 2) {  // 双击热键列
		startBindHotkey();
	}
}

// 复制功能实现
HWND MainWindow::getSelectedWindowFromCurrentTable() const
{
	// 只处理主窗口表（第0个标签页）
	if (tabWidget->currentIndex() == 0) { // 主窗口表
		int row = windowsTable->currentRow();
		if (row < 0) return nullptr;
		return reinterpret_cast<HWND>(windowsTable->item(row, 0)->data(Qt::UserRole).toULongLong());
	}
	return nullptr;
}

void MainWindow::copyTitle()
{
	HWND hwnd = getSelectedWindowFromCurrentTable();
	if (!hwnd || !IsWindow(hwnd)) return;
	
	// 使用getWindowInfo获取窗口信息，不过滤不可见字符
	WindowInfo info = getWindowInfo(hwnd, false);
	
	// 使用原始窗口标题，不过滤任何字符
	QApplication::clipboard()->setText(info.originalTitle);
}

void MainWindow::copyClass()
{
	HWND hwnd = getSelectedWindowFromCurrentTable();
	if (!hwnd || !IsWindow(hwnd)) return;
	
	// 使用getWindowInfo获取窗口信息
	WindowInfo info = getWindowInfo(hwnd);
	
	QApplication::clipboard()->setText(info.className);
}

void MainWindow::copyPath()
{
	HWND hwnd = getSelectedWindowFromCurrentTable();
	if (!hwnd || !IsWindow(hwnd)) return;
	
	// 使用getWindowInfo获取窗口信息
	WindowInfo info = getWindowInfo(hwnd);
	
	QApplication::clipboard()->setText(info.processPath);
}

void MainWindow::copyAll()
{
	HWND hwnd = getSelectedWindowFromCurrentTable();
	if (!hwnd || !IsWindow(hwnd)) return;
	
	// 使用getWindowInfo获取窗口信息，不过滤不可见字符
	WindowInfo info = getWindowInfo(hwnd, false);
	
	// 创建HTML格式的文本（类似Traynard的实现，使用翻译后的列标题）
	QString html = QString(
		"<table>"
		"<tr><th>%1</th><th>%2</th><th>%3</th><th>%4</th><th>%5</th><th>%6</th></tr>"
		"<tr>"
		"<td>%7</td>"
		"<td>0x%8</td>"
		"<td>%9</td>"
		"<td>%10</td>"
		"<td>%11</td>"
		"<td>%12</td>"
		"</tr>"
		"</table>")
		.arg(trc("MainWindow", "Window Title").toHtmlEscaped())      // 1: 窗口标题
		.arg(trc("MainWindow", "Handle").toHtmlEscaped())           // 2: 句柄
		.arg(trc("MainWindow", "Class").toHtmlEscaped())            // 3: 类
		.arg(trc("MainWindow", "Process ID").toHtmlEscaped())       // 4: 进程ID
		.arg(trc("MainWindow", "Process").toHtmlEscaped())          // 5: 进程
		.arg(trc("MainWindow", "Application Path").toHtmlEscaped()) // 6: 应用程序路径
		.arg(info.originalTitle.toHtmlEscaped())                    // 7: 窗口标题值（原始标题）
		.arg(QString::number((quintptr)hwnd, 16).toUpper())         // 8: 句柄值
		.arg(info.className.toHtmlEscaped())                        // 9: 类值
		.arg(info.processId)                                        // 10: 进程ID值
		.arg(info.processName.toHtmlEscaped())                      // 11: 进程名值
		.arg(info.processPath.toHtmlEscaped());                     // 12: 路径值
	
	// 创建纯文本格式（使用翻译后的标签）
	QString plainText = QString("%1: %2\n%3: 0x%4\n%5: %6\n%7: %8\n%9: %10\n%11: %12")
		.arg(trc("MainWindow", "Window Title"))      // 1: 窗口标题
		.arg(info.originalTitle)                     // 2: 窗口标题值（原始标题）
		.arg(trc("MainWindow", "Handle"))            // 3: 句柄
		.arg(QString::number((quintptr)hwnd, 16).toUpper()) // 4: 句柄值
		.arg(trc("MainWindow", "Class"))             // 5: 类
		.arg(info.className)                         // 6: 类值
		.arg(trc("MainWindow", "Process ID"))        // 7: 进程ID
		.arg(info.processId)                         // 8: 进程ID值
		.arg(trc("MainWindow", "Process"))           // 9: 进程
		.arg(info.processName)                       // 10: 进程名值
		.arg(trc("MainWindow", "Application Path"))  // 11: 应用程序路径
		.arg(info.processPath);                      // 12: 路径值
	
	// 设置剪贴板内容（支持HTML和纯文本）
	QMimeData* mimeData = new QMimeData();
	mimeData->setHtml(html);
	mimeData->setText(plainText);
	QApplication::clipboard()->setMimeData(mimeData);
}