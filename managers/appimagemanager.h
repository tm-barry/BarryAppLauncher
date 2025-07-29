#ifndef APPIMAGEMANAGER_H
#define APPIMAGEMANAGER_H

#pragma once

#include "models/appimagemetadata.h"
#include "utils/appimageutil.h"

#include <QDir>
#include <QObject>
#include <QUrl>

class AppImageManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AppImageMetadata* appImageMetadata READ appImageMetadata WRITE setAppImageMetadata NOTIFY appImageMetadataChanged)
    Q_PROPERTY(bool busy READ busy WRITE setBusy NOTIFY busyChanged)
    Q_PROPERTY(AppState state READ state WRITE setState NOTIFY stateChanged)
public:
    static AppImageManager* instance();

    enum AppState {
        AppInfo,
        AppList
    };
    Q_ENUM(AppState);

    AppImageMetadata* appImageMetadata() const;
    void setAppImageMetadata(AppImageMetadata* value);

    bool busy() const;
    void setBusy(bool value);

    AppState state() const;
    void setState(AppState value);

    Q_INVOKABLE void loadAppImageMetadata(const QUrl& url);
    Q_INVOKABLE void loadAppImageMetadata(const QString& path);
    Q_INVOKABLE void launchAppImage(const QUrl& url);
    Q_INVOKABLE void launchAppImage(const QString& path);
    Q_INVOKABLE void registerAppImage(const QUrl& url);
    Q_INVOKABLE void registerAppImage(const QString& path);
    Q_INVOKABLE void unregisterAppImage(const QUrl& url);
    Q_INVOKABLE void unregisterAppImage(const QString& path);

private:
    explicit AppImageManager(QObject *parent = nullptr);

    static const QRegularExpression invalidChars;
    AppImageMetadata* m_appImageMetadata = nullptr;
    bool m_busy = false;
    AppState m_state = AppList;

    AppImageMetadata* parseAppImageMetadata(const AppImageUtilMetadata& appImageMetadata);
    QString findNextAvailableFilename(const QString& fullPath);
    QString handleIntegrationFileOperation(const QString& path);

    Q_DISABLE_COPY(AppImageManager);

signals:
    void appImageMetadataChanged(AppImageMetadata* newValue);
    void busyChanged(bool newValue);
    void stateChanged(AppImageManager::AppState newValue);
};

#endif // APPIMAGEMANAGER_H
