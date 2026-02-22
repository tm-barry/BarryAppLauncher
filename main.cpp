#include "managers/appimagemanager.h"
#include "managers/clipboardmanager.h"
#include "managers/errormanager.h"
#include "managers/settingsmanager.h"
#include "managers/updatepresetmanager.h"
#include "providers/memoryimageprovider.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQuickControls2/QQuickStyle>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Fusion");
    app.setOrganizationName("tm-barry");
    app.setApplicationName("BarryAppLauncher");
    app.setApplicationVersion(APP_VERSION);
    app.setWindowIcon(QIcon(":/assets/icons/barryapplauncher.svg"));

    QString fileArg;
    QStringList args = QCoreApplication::arguments();
    if (args.size() > 1) {
        fileArg = args.at(1);
    }

    qmlRegisterType<AppImageMetadata>("BarryAppLauncher", 1, 0, "AppImageMetadata");
    qRegisterMetaType<AppImageManager::AppState>("AppImageManager::AppState");
    qRegisterMetaType<AppImageManager::ModalTypes>("AppImageManager::ModalTypes");
    qRegisterMetaType<ErrorManager::MessageType>("ErrorManager::MessageType");
    qRegisterMetaType<QQmlListProperty<UpdaterReleaseModel>>("QQmlListProperty<UpdaterReleaseModel>");
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
    engine.rootContext()->setContextProperty("FileArg", fileArg);

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

    qmlRegisterSingletonType<UpdatePresetManager>(
        "BarryAppLauncher", 1, 0, "UpdatePresetManager",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            auto* instance = UpdatePresetManager::instance();
            QQmlEngine::setObjectOwnership(instance, QQmlEngine::CppOwnership);
            return instance;
        }
        );

    auto* memoryImageProvider = MemoryImageProvider::instance();
    engine.addImageProvider(MemoryImageProvider::providerName, memoryImageProvider);

    engine.loadFromModule("BarryAppLauncher", "Main");

    return app.exec();
}
