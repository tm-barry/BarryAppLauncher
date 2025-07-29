#include "appimagemetadata.h"

// Constructor
AppImageMetadata::AppImageMetadata(QObject* parent)
    : QObject(parent)
    , m_type(0)
    , m_integration(None)
{}

// Getters and setters with change notifications

QString AppImageMetadata::name() const { return m_name; }
void AppImageMetadata::setName(const QString& value) {
    if (m_name != value) {
        m_name = value;
        emit nameChanged();
    }
}

QString AppImageMetadata::version() const { return m_version; }
void AppImageMetadata::setVersion(const QString& value) {
    if (m_version != value) {
        m_version = value;
        emit versionChanged();
    }
}

QString AppImageMetadata::comment() const { return m_comment; }
void AppImageMetadata::setComment(const QString& value) {
    if (m_comment != value) {
        m_comment = value;
        emit commentChanged();
    }
}

int AppImageMetadata::type() const { return m_type; }
void AppImageMetadata::setType(int value) {
    if (m_type != value) {
        m_type = value;
        emit typeChanged();
    }
}

QUrl AppImageMetadata::icon() const { return m_icon; }
void AppImageMetadata::setIcon(const QUrl& value) {
    if (m_icon != value) {
        m_icon = value;
        emit iconChanged();
    }
}

QString AppImageMetadata::md5() const { return m_md5; }
void AppImageMetadata::setMd5(const QString& value) {
    if (m_md5 != value) {
        m_md5 = value;
        emit md5Changed();
    }
}

QString AppImageMetadata::categories() const { return m_categories; }
void AppImageMetadata::setCategories(const QString& value) {
    if (m_categories != value) {
        m_categories = value;
        emit categoriesChanged();
    }
}

QString AppImageMetadata::path() const { return m_path; }
void AppImageMetadata::setPath(const QString& value) {
    if (m_path != value) {
        m_path = value;
        emit pathChanged();
    }
}

AppImageMetadata::IntegrationType AppImageMetadata::integration() const { return m_integration; }
void AppImageMetadata::setIntegration(IntegrationType value) {
    if (m_integration != value) {
        m_integration = value;
        emit integrationChanged();
    }
}

QString AppImageMetadata::desktopFilePath() const { return m_desktopFilePath; }
void AppImageMetadata::setDesktopFilePath(const QString& value) {
    if (m_desktopFilePath != value) {
        m_desktopFilePath = value;
        emit desktopFilePathChanged();
    }
}
