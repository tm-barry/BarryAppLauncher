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
    Q_PROPERTY(QString path MEMBER path)
    Q_PROPERTY(QString desktopFilePath MEMBER desktopFilePath)
public:
    QString name;
    QString version;
    int type;
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
    const QStringList searchPaths;

    int getAppImageType(const QString &path);
    QString findDesktopFileForExecutable(const QUrl& executableUrl);
    QString findDesktopFileForExecutable(const QString& executablePath);

signals:
};

#endif // APPIMAGEMANAGER_H
