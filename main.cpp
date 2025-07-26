#include "managers/appimagemanager.h"
#include "managers/errormanager.h"
#include "providers/memoryimageprovider.h"

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

    qRegisterMetaType<AppImageManager::AppState>("AppImageManager::AppState");
    qRegisterMetaType<ErrorManager::MessageType>("ErrorManager::MessageType");

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    qmlRegisterSingletonType<AppImageManager>(
        "BarryAppLauncher", 1, 0, "AppImageManager",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            auto* instance = AppImageManager::instance();
            QQmlEngine::setObjectOwnership(instance, QQmlEngine::CppOwnership);
            return instance;
        }
    );

    qmlRegisterSingletonType<ErrorManager>(
        "BarryAppLauncher", 1, 0, "ErrorManager",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            auto* instance = ErrorManager::instance();
            QQmlEngine::setObjectOwnership(instance, QQmlEngine::CppOwnership);
            return instance;
        }
    );

    auto* memoryImageProvider = MemoryImageProvider::instance();
    engine.addImageProvider(MemoryImageProvider::providerName, memoryImageProvider);

    engine.loadFromModule("BarryAppLauncher", "Main");

    return app.exec();
}
