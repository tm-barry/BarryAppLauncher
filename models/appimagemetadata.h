#ifndef APPIMAGEMETADATA_H
#define APPIMAGEMETADATA_H

#pragma once

#include <QObject>
#include <QUrl>

class AppImageMetadata : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY versionChanged)
    Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY commentChanged)
    Q_PROPERTY(int type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QUrl icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QString md5 READ md5 WRITE setMd5 NOTIFY md5Changed)
    Q_PROPERTY(QString categories READ categories WRITE setCategories NOTIFY categoriesChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(IntegrationType integration READ integration WRITE setIntegration NOTIFY integrationChanged)
    Q_PROPERTY(QString desktopFilePath READ desktopFilePath WRITE setDesktopFilePath NOTIFY desktopFilePathChanged)

public:
    explicit AppImageMetadata(QObject* parent = nullptr);

    enum IntegrationType {
        None,
        Internal,
        External
    };
    Q_ENUM(IntegrationType)

    // Getters and Setters
    QString name() const;
    void setName(const QString& value);

    QString version() const;
    void setVersion(const QString& value);

    QString comment() const;
    void setComment(const QString& value);

    int type() const;
    void setType(int value);

    QUrl icon() const;
    void setIcon(const QUrl& value);

    QString md5() const;
    void setMd5(const QString& value);

    QString categories() const;
    void setCategories(const QString& value);

    QString path() const;
    void setPath(const QString& value);

    IntegrationType integration() const;
    void setIntegration(IntegrationType value);

    QString desktopFilePath() const;
    void setDesktopFilePath(const QString& value);

signals:
    void nameChanged();
    void versionChanged();
    void commentChanged();
    void typeChanged();
    void iconChanged();
    void md5Changed();
    void categoriesChanged();
    void pathChanged();
    void integrationChanged();
    void desktopFilePathChanged();

private:
    QString m_name;
    QString m_version;
    QString m_comment;
    int m_type = 0;
    QUrl m_icon;
    QString m_md5;
    QString m_categories;
    QString m_path;
    IntegrationType m_integration = None;
    QString m_desktopFilePath;
};

#endif // APPIMAGEMETADATA_H
