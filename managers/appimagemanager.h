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
    Q_PROPERTY(QString desktopFilePath MEMBER desktopFilePath)
public:
    QString name;
    QString version;
    int type;
    QString md5;
    QString categories;
    QString path;
    QString desktopFilePath;
};

class AppImageManager : public QObject
{
    Q_OBJECT
public:
    explicit AppImageManager(QObject *parent = nullptr);

    Q_INVOKABLE AppImageMetadata getAppImageMetadata(const QUrl& url);
    Q_INVOKABLE AppImageMetadata getAppImageMetadata(const QString& path);

private:
    QString getDesktopFileForExecutable(const QString& executablePath);
    QString getInternalAppImageDesktopContent(const QString& appImagePath);
    QString getExternalAppImageDesktopContent(const QString& desktopPath);
    void loadMetadataFromDesktopContent(AppImageMetadata& appImageMetadata, const QString& desktopContent);

signals:
};

#endif // APPIMAGEMANAGER_H
