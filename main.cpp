#include "managers/appimagemanager.h"
#include "managers/clipboardmanager.h"
#include "managers/errormanager.h"
#include "managers/settingsmanager.h"
#include "providers/memoryimageprovider.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQuickControls2/QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Fusion");
    app.setOrganizationName("tm-barry");
    app.setApplicationName("BarryAppLauncher");
    QCoreApplication::setApplicationVersion("0.5.0");
    app.setWindowIcon(QIcon(":/assets/icons/barry-app-launcher.svg"));

    qmlRegisterType<AppImageMetadata>("BarryAppLauncher", 1, 0, "AppImageMetadata");
    qRegisterMetaType<AppImageManager::AppState>("AppImageManager::AppState");
    qRegisterMetaType<AppImageManager::ModalTypes>("AppImageManager::ModalTypes");
    qRegisterMetaType<ErrorManager::MessageType>("ErrorManager::MessageType");
    qmlRegisterUncreatableType<AppImageMetadata>("BarryAppLauncher", 1, 0, "AppImageMetadata",
                                                 "Enum only - AppImageMetadata is not instantiable");

    // Create the singleton BEFORE any threads are used     
    AppImageManager::instance();
    ClipboardManager::instance();
    ErrorManager::instance();
    SettingsManager::instance();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("AppName", app.applicationName());
    engine.rootContext()->setContextProperty("AppVersion", app.applicationVersion());

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

    qmlRegisterSingletonType<ClipboardManager>(
        "BarryAppLauncher", 1, 0, "ClipboardManager",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            auto* instance = ClipboardManager::instance();
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

    qmlRegisterSingletonType<SettingsManager>(
        "BarryAppLauncher", 1, 0, "SettingsManager",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            auto* instance = SettingsManager::instance();
            QQmlEngine::setObjectOwnership(instance, QQmlEngine::CppOwnership);
            return instance;
        }
        );

    auto* memoryImageProvider = MemoryImageProvider::instance();
    engine.addImageProvider(MemoryImageProvider::providerName, memoryImageProvider);

    engine.loadFromModule("BarryAppLauncher", "Main");

    return app.exec();
}
