#include "appimagemanager.h"

#include <QDir>
#include <QObject>
#include <QUrl>

const QStringList searchPaths = {
    "/usr/share/applications",
    "/usr/local/share/applications",
    QDir::homePath() + "/.local/share/applications"
};

AppImageManager::AppImageManager(QObject *parent)
    : QObject{parent}
{}

AppImageMetadata AppImageManager::getAppImageMetadata(const QUrl& url) {
    return getAppImageMetadata(url.toLocalFile());
}

AppImageMetadata AppImageManager::getAppImageMetadata(const QString& path) {
    AppImageMetadata appImageMetadata;
    appImageMetadata.path = path;
    appImageMetadata.type = getAppImageType(path);

    return appImageMetadata;
}

int AppImageManager::getAppImageType(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return 0;  // 0 = Unknown or error

    if (!file.seek(8))
        return 0;

    QByteArray magic = file.read(3);
    if (magic == "AI\x01")
        return 1; // Type 1
    else if (magic == "AI\x02")
        return 2; // Type 2
    else
        return 0; // Unknown
}

QString AppImageManager::findDesktopFileForExecutable(const QUrl& executableUrl) {
    return findDesktopFileForExecutable(executableUrl.toLocalFile());
}

QString AppImageManager::findDesktopFileForExecutable(const QString& executablePath) {
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
