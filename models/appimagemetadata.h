#ifndef APPIMAGEMETADATA_H
#define APPIMAGEMETADATA_H

#pragma once

#include "models/updaterfiltermodel.h"
#include "models/updaterreleasemodel.h"

#include <QObject>
#include <QUrl>
#include <QQmlListProperty>

class AppImageMetadata : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY versionChanged)
    Q_PROPERTY(QString comment READ comment WRITE setComment NOTIFY commentChanged)
    Q_PROPERTY(int type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QUrl icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QString checksum READ checksum WRITE setChecksum NOTIFY checksumChanged)
    Q_PROPERTY(QString categories READ categories WRITE setCategories NOTIFY categoriesChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(IntegrationType integration READ integration WRITE setIntegration NOTIFY integrationChanged)
    Q_PROPERTY(QString desktopFilePath READ desktopFilePath WRITE setDesktopFilePath NOTIFY desktopFilePathChanged)
    Q_PROPERTY(bool executable READ executable WRITE setExecutable NOTIFY executableChanged)
    Q_PROPERTY(QString updateType READ updateType WRITE setUpdateType NOTIFY updateTypeChanged)
    Q_PROPERTY(QString updateUrl READ updateUrl WRITE setUpdateUrl NOTIFY updateUrlChanged)
    Q_PROPERTY(QString updateDownloadField READ updateDownloadField WRITE setUpdateDownloadField NOTIFY updateDownloadFieldChanged)
    Q_PROPERTY(QString updateDownloadPattern READ updateDownloadPattern WRITE setUpdateDownloadPattern NOTIFY updateDownloadPatternChanged)
    Q_PROPERTY(QString updateDateField READ updateDateField WRITE setUpdateDateField NOTIFY updateDateFieldChanged)
    Q_PROPERTY(QString updateVersionField READ updateVersionField WRITE setUpdateVersionField NOTIFY updateVersionFieldChanged)
    Q_PROPERTY(QString updateVersionPattern READ updateVersionPattern WRITE setUpdateVersionPattern NOTIFY updateVersionPatternChanged)
    Q_PROPERTY(QQmlListProperty<UpdaterFilterModel> updateFilters READ updateFilters NOTIFY updateFiltersChanged)
    Q_PROPERTY(bool updateDirty READ updateDirty WRITE setUpdateDirty NOTIFY updateDirtyChanged)
    Q_PROPERTY(QQmlListProperty<UpdaterReleaseModel> updaterReleases READ updaterReleases NOTIFY updaterReleasesChanged)
    Q_PROPERTY(bool hasNewRelease READ hasNewRelease NOTIFY hasNewReleaseChanged)
    Q_PROPERTY(QString updateCurrentVersion READ updateCurrentVersion WRITE setUpdateCurrentVersion NOTIFY updateCurrentVersionChanged)
    Q_PROPERTY(QString updateCurrentDate READ updateCurrentDate WRITE setUpdateCurrentDate NOTIFY updateCurrentDateChanged)

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

    QString checksum() const;
    void setChecksum(const QString& value);

    QString categories() const;
    void setCategories(const QString& value);

    QString path() const;
    void setPath(const QString& value);

    IntegrationType integration() const;
    void setIntegration(IntegrationType value);

    QString desktopFilePath() const;
    void setDesktopFilePath(const QString& value);

    bool executable() const;
    void setExecutable(bool value);

    QString updateType() const;
    void setUpdateType(const QString& value);

    QString updateUrl() const;
    void setUpdateUrl(const QString& value);

    QString updateDownloadField() const;
    void setUpdateDownloadField(const QString& value);

    QString updateDownloadPattern() const;
    void setUpdateDownloadPattern(const QString& value);

    QString updateDateField() const;
    void setUpdateDateField(const QString& value);

    QString updateVersionField() const;
    void setUpdateVersionField(const QString& value);

    QString updateVersionPattern() const;
    void setUpdateVersionPattern(const QString& value);

    QQmlListProperty<UpdaterFilterModel> updateFilters();
    const QList<UpdaterFilterModel*>& getUpdateFilters() const { return m_updateFilters; }
    void addUpdateFilter(UpdaterFilterModel* filter);
    Q_INVOKABLE void addUpdateFilterWithValues(const QString &field, const QString &pattern);
    Q_INVOKABLE void removeUpdateFilter(int index);

    bool updateDirty() const;
    Q_INVOKABLE void setUpdateDirty(bool value);

    QQmlListProperty<UpdaterReleaseModel> updaterReleases();
    void addUpdaterRelease(UpdaterReleaseModel* release);
    void clearUpdaterReleases();

    bool hasNewRelease();

    QString updateCurrentVersion() const;
    void setUpdateCurrentVersion(const QString& value);

    QString updateCurrentDate() const;
    void setUpdateCurrentDate(const QString& value);

signals:
    void nameChanged();
    void versionChanged();
    void commentChanged();
    void typeChanged();
    void iconChanged();
    void checksumChanged();
    void categoriesChanged();
    void pathChanged();
    void integrationChanged();
    void desktopFilePathChanged();
    void executableChanged();
    void updateTypeChanged();
    void updateUrlChanged();
    void updateDownloadFieldChanged();
    void updateDownloadPatternChanged();
    void updateDateFieldChanged();
    void updateVersionFieldChanged();
    void updateVersionPatternChanged();
    void updateFiltersChanged();
    void updateDirtyChanged();
    void updaterReleasesChanged();
    void hasNewReleaseChanged();
    void updateCurrentVersionChanged();
    void updateCurrentDateChanged();

private:
    QString m_name;
    QString m_version;
    QString m_comment;
    int m_type = 0;
    QUrl m_icon;
    QString m_checksum;
    QString m_categories;
    QString m_path;
    IntegrationType m_integration = None;
    QString m_desktopFilePath;
    bool m_executable = false;
    QString m_updateType;
    QString m_updateUrl;
    QString m_updateDownloadField;
    QString m_updateDownloadPattern;
    QString m_updateDateField;
    QString m_updateVersionField;
    QString m_updateVersionPattern;
    QList<UpdaterFilterModel*> m_updateFilters;
    bool m_updateDirty = false;
    QList<UpdaterReleaseModel*> m_updaterReleases;
    QString m_updateCurrentVersion;
    QString m_updateCurrentDate;

    void onUpdateFilterChanged();
    void onUpdateFilterPropertiesChanged();

    static void appendUpdateFilter(QQmlListProperty<UpdaterFilterModel>* list, UpdaterFilterModel* filter);
    static qsizetype updateFiltersCount(QQmlListProperty<UpdaterFilterModel>* list);
    static UpdaterFilterModel* updateFilterAt(QQmlListProperty<UpdaterFilterModel>* list, qsizetype index);
    static void clearUpdateFilters(QQmlListProperty<UpdaterFilterModel>* list);

    static qsizetype updaterReleasesCount(QQmlListProperty<UpdaterReleaseModel> *list);
    static UpdaterReleaseModel* updaterReleasesAt(QQmlListProperty<UpdaterReleaseModel> *list, qsizetype index);
};

#endif // APPIMAGEMETADATA_H
