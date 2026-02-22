#include "memoryimageprovider.h"

// ----------------- Public -----------------

const QString MemoryImageProvider::providerName = "memoryimage";

MemoryImageProvider* MemoryImageProvider::instance() {
    static MemoryImageProvider* singleton = new MemoryImageProvider();
    return singleton;
}

QImage MemoryImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    QImage img = m_images.value(id);

    if (img.isNull())
        return QImage();

    if (size)
        *size = img.size();

    if (requestedSize.width() > 0 && requestedSize.height() > 0)
        img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    return img;
}

void MemoryImageProvider::setImage(const QString &id, const QImage &img) {
    m_images[id] = img;
}

QString MemoryImageProvider::getUrl(const QString &id) {
    return QString("image://%1/%2").arg(providerName, id);
}

void MemoryImageProvider::clearImages() {
    m_images.clear();
}

void MemoryImageProvider::removeImage(const QString &id) {
    m_images.remove(id);
}

// ----------------- Private -----------------

MemoryImageProvider::MemoryImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
