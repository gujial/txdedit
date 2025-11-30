#include "MainWindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QIcon>
#include <QDir>
#include <QFile>

QString findIconPath(const QString& iconName) {
    // Use Qt resource system
    QString resourcePath = ":/icons/" + iconName;
    if (QFile::exists(resourcePath)) {
        return resourcePath;
    }
    return QString();
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Set application metadata
    app.setApplicationName("TXD Edit");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("TXDEdit");
    
    // Set application icon
#ifdef Q_OS_MACOS
    QString iconPath = findIconPath("mac.icns");
#elif defined(Q_OS_WIN)
    QString iconPath = findIconPath("windows.ico");
#else
    QString iconPath = findIconPath("mac.icns"); // Fallback
#endif
    
    if (!iconPath.isEmpty()) {
        QIcon appIcon(iconPath);
        app.setWindowIcon(appIcon);
    }
    
    // Use native style
    app.setStyle(QStyleFactory::create("Fusion"));
    
    MainWindow window;
    window.setWindowIcon(app.windowIcon());
    window.show();
    
    return app.exec();
}

