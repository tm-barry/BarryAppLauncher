#ifndef APPIMAGEMANAGER_H
#define APPIMAGEMANAGER_H

#pragma once

#include <QObject>
#include <QFile>
#include <QByteArray>
#include <QDebug>
#include <QUrl>

class AppImageManager : public QObject
{
    Q_OBJECT
public:
    explicit AppImageManager(QObject *parent = nullptr) {}

    Q_INVOKABLE bool isAppImageType2(const QUrl &url) {
        QFile file(url.toLocalFile());
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Cannot open file:" << url.toLocalFile();
            return false;
        }

        if (!file.seek(8)) {
            qWarning() << "Cannot seek to magic offset";
            return false;
        }

        QByteArray magic = file.read(4);
        return magic == QByteArray::fromHex("41490200"); // "AI\x02\x00"
    }

signals:
};

#endif // APPIMAGEMANAGER_H
