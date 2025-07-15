#include "appimagemanager.h"

#include <appimage/appimage.h>
#include <appimage/core/AppImage.h>

#include <QDir>
#include <QObject>
#include <QUrl>

AppImageManager::AppImageManager(QObject *parent)
    : QObject{parent}
{}

AppImageMetadata AppImageManager::getAppImageMetadata(const QUrl& url) {
    return getAppImageMetadata(url.toLocalFile());
}

AppImageMetadata AppImageManager::getAppImageMetadata(const QString& path) {
    AppImageMetadata appImageMetadata;
    appImageMetadata.path = path;
    int type = appimage_get_type(path.toUtf8().constData(), false);
    appImageMetadata.type = type;

    if (type <= 0) {
        qWarning() << "Unsupported AppImage type";
        return appImageMetadata;
    }

    char* md5 = appimage_get_md5(path.toUtf8().constData());
    appImageMetadata.md5 = QString::fromUtf8(md5);
    QString integratedDesktopPath = getDesktopFileForExecutable(path);

    QString desktopContent;
    if(integratedDesktopPath == nullptr) {
        appImageMetadata.integrated = false;
        desktopContent = getInternalAppImageDesktopContent(path);
    }
    else
    {
        appImageMetadata.integrated = true;
        appImageMetadata.desktopFilePath = integratedDesktopPath;
        desktopContent = getExternalAppImageDesktopContent(integratedDesktopPath);
    }
    loadMetadataFromDesktopContent(appImageMetadata, desktopContent);

    qDebug() << appImageMetadata.name;
    return appImageMetadata;
}

QString AppImageManager::getDesktopFileForExecutable(const QString& executablePath) {
    const QStringList searchPaths = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        QDir::homePath() + "/.local/share/applications"
    };

    for (const QString& dirPath : searchPaths) {
        QDir dir(dirPath);
        const QStringList desktopFiles = dir.entryList(QStringList() << "*.desktop", QDir::Files);

        for (const QString& fileName : desktopFiles) {
            QString filePath = dir.absoluteFilePath(fileName);
            QFile file(filePath);

            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();

                if (line.startsWith("Exec=")) {
                    QString execLine = line.mid(QString("Exec=").length());
                    const QStringList execCommandParts = execLine.split(' ');

                    for (const QString& execCommand : execCommandParts)
                    {
                        if (execCommand == executablePath) {
                            return filePath;
                        }
                    }
                }
            }
        }
    }
    return QString();
}

QString AppImageManager::getInternalAppImageDesktopContent(const QString& appImagePath)
{
    QString desktopContent;
    QString desktopPath;
    char** files = appimage_list_files(appImagePath.toUtf8().constData());
    for (int i = 0; files[i] != nullptr; ++i) {
        QString entry = QString::fromUtf8(files[i]);
        if (entry.endsWith(".desktop")) {
            desktopPath = entry;
            break;
        }
    }

    appimage_string_list_free(files);

    if (desktopPath.isEmpty()) {
        qWarning() << "No .desktop file found";
    }
    else {
        char* buffer = nullptr;
        unsigned long bufferSize = 0;
        bool success = appimage_read_file_into_buffer_following_symlinks(
            appImagePath.toUtf8().constData(),
            desktopPath.toUtf8().constData(),
            &buffer,
            &bufferSize
            );

        if (!success || !buffer || bufferSize == 0) {
            qWarning() << "Failed to read .desktop file contents.";
            return {};
        }

        desktopContent = QString::fromUtf8(QByteArray(buffer, bufferSize));
        free(buffer);
    }

    return desktopContent;
}

QString AppImageManager::getExternalAppImageDesktopContent(const QString& desktopPath)
{
    QFile file(desktopPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    return content;
}

void AppImageManager::loadMetadataFromDesktopContent(AppImageMetadata& appImageMetadata, const QString& desktopContent)
{
    const QStringList lines = desktopContent.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("Name=")) {
            appImageMetadata.name = trimmed.section('=', 1).trimmed();
        }
        else if (trimmed.startsWith("X-AppImage-Version=")) {
            appImageMetadata.version = trimmed.section('=', 1).trimmed();
        }
        else if (trimmed.startsWith("Categories=")) {
            appImageMetadata.categories = trimmed.section('=', 1).trimmed();
        }
    }
}
