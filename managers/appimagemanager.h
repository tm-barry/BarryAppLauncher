#ifndef APPIMAGEMANAGER_H
#define APPIMAGEMANAGER_H

#pragma once

#include <QDir>
#include <QObject>
#include <QFile>
#include <QByteArray>
#include <QDebug>
#include <QUrl>

enum AppImageType {
    Type1 = 1,
    Type2 = 2
};

enum AppImageUpdateType {
    Static,
    Github
};

struct EnvironmentVariable {
    std::string name;
    std::string value;
};

struct AppImage {
    std::string name;
    std::string version;
    AppImageType type;
    std::string path;
    std::string desktopFilePath;
    std::string commandLineArgs;
    AppImageUpdateType updateType;
    std::string updateUrl;
    std::string websiteUrl;
    std::vector<EnvironmentVariable> environmentVariables;
};

class AppImageManager : public QObject
{
    Q_OBJECT
private:
    const QStringList searchPaths = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        QDir::homePath() + "/.local/share/applications"
    };

public:
    explicit AppImageManager(QObject *parent = nullptr) {}

    Q_INVOKABLE bool isAppImageType2(const QUrl& fileUrl) {
        return isAppImageType2(fileUrl.toLocalFile());
    }

    Q_INVOKABLE bool isAppImageType2(const QString& filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Cannot open file:" << filePath;
            return false;
        }

        if (!file.seek(8)) {
            qWarning() << "Cannot seek to magic offset";
            return false;
        }

        QByteArray magic = file.read(4);
        return magic == QByteArray::fromHex("41490200"); // "AI\x02\x00"
    }

    Q_INVOKABLE QString findDesktopFileForExecutable(const QUrl& executableUrl) {
        return findDesktopFileForExecutable(executableUrl.toLocalFile());
    }

    Q_INVOKABLE QString findDesktopFileForExecutable(const QString& executablePath) {
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

signals:
};

#endif // APPIMAGEMANAGER_H
