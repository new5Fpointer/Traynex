#include <QApplication>
#include <QSystemTrayIcon>
#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // 检查系统托盘是否可用
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qCritical() << "System tray is not available on this system.";
        return 1;
    }

    // 设置应用程序属性
    app.setQuitOnLastWindowClosed(false);

    MainWindow w;
    return app.exec();
}