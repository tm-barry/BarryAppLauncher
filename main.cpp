#include "managers/appimagemanager.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQuickControls2/QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Fusion");
    app.setOrganizationName("tm-barry");
    app.setApplicationName("BarryAppLauncher");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    AppImageManager appImageManager;
    engine.rootContext()->setContextProperty("AppImageManager", &appImageManager);

    engine.loadFromModule("BarryAppLauncher", "Main");

    return app.exec();
}
