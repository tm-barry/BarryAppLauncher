#ifndef MEMORYIMAGEPROVIDER_H
#define MEMORYIMAGEPROVIDER_H

#pragma once

#include <QQuickImageProvider>
#include <QImage>
#include <QHash>
#include <QString>

class MemoryImageProvider : public QQuickImageProvider
{
public:
    static MemoryImageProvider* instance();
    static const QString providerName;

    // Override requestImage to provide images by id
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    // Method to set or update an image for a given id
    void setImage(const QString &id, const QImage &image);

    QString getUrl(const QString &id);

private:
    MemoryImageProvider();

    QHash<QString, QImage> m_images;

    Q_DISABLE_COPY(MemoryImageProvider);
};

#endif // MEMORYIMAGEPROVIDER_H
