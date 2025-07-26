#ifndef APPIMAGEMANAGER_H
#define APPIMAGEMANAGER_H

#pragma once

#include <QDir>
#include <QObject>
#include <QUrl>

struct AppImageMetadata {
    Q_GADGET

    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString version MEMBER version)
    Q_PROPERTY(int type MEMBER type)
    Q_PROPERTY(QUrl icon MEMBER icon)
    Q_PROPERTY(QString md5 MEMBER md5)
    Q_PROPERTY(QString categories MEMBER categories)
    Q_PROPERTY(QString path MEMBER path)
    Q_PROPERTY(bool integrated MEMBER integrated)
    Q_PROPERTY(QString desktopFilePath MEMBER desktopFilePath)
public:
    QString name;
    QString version;
    int type;
    QUrl icon;
    QString md5;
    QString categories;
    QString path;
    bool integrated;
    QString desktopFilePath;
};

class AppImageManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AppImageMetadata appImageMetadata READ appImageMetadata NOTIFY appImageMetadataChanged)
    Q_PROPERTY(bool busy READ busy WRITE setBusy NOTIFY busyChanged)
    Q_PROPERTY(AppState state READ state WRITE setState NOTIFY stateChanged)
public:
    static AppImageManager* instance();

    enum AppState {
        AppInfo,
        AppList
    };
    Q_ENUM(AppState);

    AppImageMetadata appImageMetadata();
    void setAppImageMetadata(AppImageMetadata value);

    bool busy() const;
    void setBusy(bool value);

    AppState state() const;
    void setState(AppState value);

    Q_INVOKABLE void loadAppImageMetadata(const QUrl& url);
    Q_INVOKABLE void loadAppImageMetadata(const QString& path);

private:
    explicit AppImageManager(QObject *parent = nullptr);

    AppImageMetadata m_appImageMetadata;
    bool m_busy = false;
    AppState m_state = AppList;

    AppImageMetadata getAppImageMetadata(const QString& path);
    QString getDesktopFileForExecutable(const QString& executablePath);
    QString getInternalAppImageDesktopContent(const QString& appImagePath);
    QString getExternalAppImageDesktopContent(const QString& desktopPath);
    QImage getAppImageIcon(const QString& path);
    void loadMetadataFromDesktopContent(AppImageMetadata& appImageMetadata, const QString& desktopContent);

    Q_DISABLE_COPY(AppImageManager);

signals:
    void appImageMetadataChanged(AppImageMetadata newValue);
    void busyChanged(bool newValue);
    void stateChanged(AppImageManager::AppState newValue);
};

#endif // APPIMAGEMANAGER_H
