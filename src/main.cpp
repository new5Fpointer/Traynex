#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMainWindow>
#include <QDebug>
#include <QMessagebox>
#include "mainwindow.h"
#include "windowstraymanager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 创建托盘图标
    QSystemTrayIcon trayIcon;
    trayIcon.setIcon(QIcon(":/icon/icon.png"));

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qCritical() << "System tray is not available on this system.";
        return 1;
    }

    if (!WindowsTrayManager::instance().initialize()) {
        QMessageBox::critical(nullptr, "Error",
            "Initialization failed");
        return 1;
    }

    // 显示托盘图标并保持程序运行
    trayIcon.show();
    // 设置应用程序属性
    app.setQuitOnLastWindowClosed(false);

    MainWindow w;
    return app.exec();
}
