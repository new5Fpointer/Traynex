#include <QApplication>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QIcon>
#include <QDebug>
#include <QDir>
#include "mainwindow.h"
#include "windowstraymanager.h"

int main(int argc, char* argv[])
{
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    QApplication app(argc, argv);

    // 设置应用程序属性
    app.setApplicationName("Traynex");
    app.setApplicationVersion("1.0.0");
    app.setQuitOnLastWindowClosed(false);

    // 设置应用程序图标
    QIcon appIcon(":/icon/icon.png");
    app.setWindowIcon(appIcon);

    // 检查系统托盘是否可用
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qCritical() << "System tray is not available on this system.";
        QMessageBox::critical(nullptr, "Error",
            "System tray is not available on this system.");
        return 1;
    }

    // 初始化 Windows 托盘管理器
    if (!WindowsTrayManager::instance().initialize()) {
        QMessageBox::critical(nullptr, "Error",
            "Failed to initialize Windows tray manager");
        return 1;
    }

    // 创建主窗口
    MainWindow w;

    return app.exec();
}