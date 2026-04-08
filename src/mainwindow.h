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
#include <QMap>
#include <QLineEdit>
#include <windows.h>

class MainWindow : public QMainWindow {
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
	void onRefreshSettingChanged();
	void onLanguageChanged();
	void onStartWithSystemChanged();
	void autoSaveSettings();
	void onAlwaysOnTopChanged();
	void highlightWindow();
	void toggleWindowOnTop();
	void refreshHiddenWindowsTable();
	void restoreSelectedHiddenWindow();
	void onHiddenTableContextMenu(const QPoint& pos);
	void updateTrayMenu();
	void hideToAppTray();
	void restoreWindowFromAppTray();
	void restoreLastWindow();
	void onHotkeyTriggered(const QString& id);
	void onOpacitySliderChanged(int value);
	void openFileLocation();
	void showFileProperties();
	void onResetDefaults();
	
	// 复制功能
	HWND getSelectedWindowFromCurrentTable() const;
	void copyTitle();
	void copyClass();
	void copyPath();
	void copyAll();

	void onHotkeySelectionChanged();
	void startBindHotkey();
	void clearSelectedHotkey();
	void onHotkeyItemDoubleClicked(QTableWidgetItem* item);

	// 表头右键菜单功能
	void onTableHeaderContextMenu(const QPoint& pos);
	void onHiddenTableHeaderContextMenu(const QPoint& pos);
	void toggleColumnVisibility(int column);
	void resetTableColumnWidths(QTableWidget* table);

protected:
	void closeEvent(QCloseEvent* event) override;
	bool eventFilter(QObject* obj, QEvent* event) override;

private:
	void createTrayIcon();
	void createHeaderContextMenu();
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

	void addWindowToTrayMenu(HWND hwnd, const QString& title, const QIcon& icon = QIcon());
	void removeWindowFromTrayMenu(HWND hwnd);
	void updateTrayMenuLayout();
	void updateTrayMenuIcons();

	void saveHotkeySettings();
	void loadHotkeySettings();

	void finishHotkeySetting(const QString& keySequence);
	void cancelHotkeySetting();

	void toggleMuteWindow();

	void createDefaultConfig();

	void initializeHotkeyTable();
	bool isHotkeyAvailable(const QKeySequence& keySequence);

	QIcon getWindowIcon(HWND hwnd) const;

	struct WindowInfo {
		QString title;
		QString originalTitle;  // 原始标题，不过滤字符
		QString processName;
		QString processPath;    // 完整进程路径
		QString className;
		DWORD processId;
		HWND hwnd;
		bool isHidden;
		bool isVisible;
		QIcon icon;
	};
	QList<QPair<HWND, WindowInfo>> getAllWindowsInfo() const;
	WindowInfo getWindowInfo(HWND hwnd, bool filterInvisibleChars = true) const;
	QList<QPair<HWND, WindowInfo>> m_lastWindowsInfo;
	QList<HWND> m_hiddenWindowOrder;
	QMap<DWORD, bool> muteStates;

	// 配置文件路径
	QString getConfigPath() const;
	HWND getSelectedWindow() const;

	// UI 组件
	QTabWidget* tabWidget;

	// 主页面组件
	QTableWidget* windowsTable;

	// 主页面右键菜单
	QMenu* contextMenu;
	QAction* hideToTrayAction;
	QAction* hideToAppTrayAction;
	QAction* bringToFrontAction;
	QAction* highlightAction;
	QAction* toggleOnTopAction;
	QAction* muteAction;
	QAction* opacityAction;
	QAction* openFolderAction;
	QAction* filePropsAction;
	QAction* endTaskAction;
	
	// 复制功能菜单
	QMenu* copyMenu;
	QAction* copyTitleAction;
	QAction* copyClassAction;
	QAction* copyPathAction;
	QAction* copyAllAction;

	// 音量子控件
	QMenu* opacityMenu;
	QSlider* opacitySlider;
	QLabel* opacityLabel;

	// 隐藏窗口页面组件
	QTableWidget* hiddenWindowsTable;

	// 隐藏窗口页面右键菜单
	QMenu* hiddenTableContextMenu;
	QAction* restoreHiddenAction;
	QAction* restoreLastHiddenAction;
	QAction* restoreAllHiddenAction;

	// 表头右键菜单
	QMenu* headerContextMenu;
	QAction* showIconColumnAction;
	QAction* showTitleColumnAction;
	QAction* showHandleColumnAction;
	QAction* showClassColumnAction;
	QAction* showPidColumnAction;
	QAction* showProcessColumnAction;
	QAction* resetColumnWidthsAction;

	// 设置页面组件
	QCheckBox* startWithSystemCheck;
	QCheckBox* enableHotkeyCheck;
	QComboBox* languageCombo;
	QPushButton* saveSettingsButton;
	QCheckBox* alwaysOnTopCheck;
	QCheckBox* autoRefreshCheck;
	QSpinBox* refreshIntervalSpin;
	QPushButton* resetDefaultsButton;

	// 热键设置相关
	QTableWidget* hotkeyTable;
	QPushButton* bindHotkeyButton;
	QPushButton* clearHotkeyButton;
	QAction* currentHotkeyAction;

	// 关于页面组件
	QLabel* aboutLabel;

	// 托盘相关
	QSystemTrayIcon* trayIcon;
	QMenu* trayMenu;
	QAction* showAction;
	QMap<HWND, QAction*> m_appTrayWindows;
	QAction* restoreLastAction;
	QAction* restoreAllAction;
	QAction* quitAction;

	// 定时刷新计时器
	QTimer* refreshTimer;

	// 热键设置状态
	bool m_settingHotkey = false;
	QString m_currentHotkeyId;
};