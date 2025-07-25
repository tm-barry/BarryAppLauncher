#ifndef APPIMAGEMANAGER_H
#define APPIMAGEMANAGER_H

#pragma once

#include <QDir>
#include <QObject>

struct AppImageMetadata {
    Q_GADGET

    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString version MEMBER version)
    Q_PROPERTY(int type MEMBER type)
    Q_PROPERTY(QString md5 MEMBER md5)
    Q_PROPERTY(QString categories MEMBER categories)
    Q_PROPERTY(QString path MEMBER path)
    Q_PROPERTY(bool integrated MEMBER integrated)
    Q_PROPERTY(QString desktopFilePath MEMBER desktopFilePath)
public:
    QString name;
    QString version;
    int type;
    QString md5;
    QString categories;
    QString path;
    bool integrated;
    QString desktopFilePath;
};

class AppImageManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(AppState state READ state WRITE setState NOTIFY stateChanged)
public:
    static AppImageManager* instance();

    enum AppState {
        AppInfo,
        AppList
    };
    Q_ENUM(AppState);

    bool busy() const;

    AppState state() const;
    void setState(AppState value);

    Q_INVOKABLE AppImageMetadata getAppImageMetadata(const QUrl& url);
    Q_INVOKABLE AppImageMetadata getAppImageMetadata(const QString& path);

private:
    explicit AppImageManager(QObject *parent = nullptr);

    bool m_busy = false;
    AppState m_state = AppInfo;
    void setBusy(bool value);

    QString getDesktopFileForExecutable(const QString& executablePath);
    QString getInternalAppImageDesktopContent(const QString& appImagePath);
    QString getExternalAppImageDesktopContent(const QString& desktopPath);
    void loadMetadataFromDesktopContent(AppImageMetadata& appImageMetadata, const QString& desktopContent);

    Q_DISABLE_COPY(AppImageManager);

signals:
    void busyChanged(bool newValue);
    void stateChanged(AppImageManager::AppState newValue);
};

#endif // APPIMAGEMANAGER_H
