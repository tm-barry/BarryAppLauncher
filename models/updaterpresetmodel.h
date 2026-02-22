#ifndef UPDATERPRESETMODEL_H
#define UPDATERPRESETMODEL_H

#include "models/updaterfiltermodel.h"

#include <QObject>
#include <QQmlListProperty>

class UpdaterPresetModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QString versionField READ versionField WRITE setVersionField NOTIFY versionFieldChanged)
    Q_PROPERTY(QString versionPattern READ versionPattern WRITE setVersionPattern NOTIFY versionPatternChanged)
    Q_PROPERTY(QString downloadField READ downloadField WRITE setDownloadField NOTIFY downloadFieldChanged)
    Q_PROPERTY(QString downloadPattern READ downloadPattern WRITE setDownloadPattern NOTIFY downloadPatternChanged)
    Q_PROPERTY(QString dateField READ dateField WRITE setDateField NOTIFY dateFieldChanged)
    Q_PROPERTY(QQmlListProperty<UpdaterFilterModel> filters READ filters NOTIFY filtersChanged)
    Q_PROPERTY(bool isSystemPreset READ isSystemPreset WRITE setIsSystemPreset NOTIFY isSystemPresetChanged)

public:
    explicit UpdaterPresetModel(QObject* parent = nullptr)
        : QObject(parent) {}

    static UpdaterPresetModel* createFromUtil(const UpdaterSettingsPreset& preset, bool isSystemPreset = false, QObject* parent = nullptr)
    {
        auto* presetModel = new UpdaterPresetModel(parent);
        presetModel->setName(preset.name);
        presetModel->setType(preset.settings.type);
        presetModel->setUrl(preset.settings.url);
        presetModel->setVersionField(preset.settings.versionField);
        presetModel->setVersionPattern(preset.settings.versionPattern);
        presetModel->setDownloadField(preset.settings.downloadField);
        presetModel->setDownloadPattern(preset.settings.downloadPattern);
        presetModel->setDateField(preset.settings.dateField);

        for (const auto& filter : preset.settings.filters) {
            presetModel->addFilter(UpdaterFilterModel::createFromUtil(filter, presetModel));
        }

        presetModel->setIsSystemPreset(isSystemPreset);

        return presetModel;
    }

    QString name() const { return m_name; }
    void setName(const QString& v) { if (m_name != v) { m_name = v; emit nameChanged(); } }

    QString type() const { return m_type; }
    void setType(const QString& v) { if (m_type != v) { m_type = v; emit typeChanged(); } }

    QString url() const { return m_url; }
    void setUrl(const QString& v) { if (m_url != v) { m_url = v; emit urlChanged(); } }

    QString versionField() const { return m_versionField; }
    void setVersionField(const QString& v) { if (m_versionField != v) { m_versionField = v; emit versionFieldChanged(); } }

    QString versionPattern() const { return m_versionPattern; }
    void setVersionPattern(const QString& v) { if (m_versionPattern != v) { m_versionPattern = v; emit versionPatternChanged(); } }

    QString downloadField() const { return m_downloadField; }
    void setDownloadField(const QString& v) { if (m_downloadField != v) { m_downloadField = v; emit downloadFieldChanged(); } }

    QString downloadPattern() const { return m_downloadPattern; }
    void setDownloadPattern(const QString& v) { if (m_downloadPattern != v) { m_downloadPattern = v; emit downloadPatternChanged(); } }

    QString dateField() const { return m_dateField; }
    void setDateField(const QString& v) { if (m_dateField != v) { m_dateField = v; emit dateFieldChanged(); } }

    const QList<UpdaterFilterModel*>& getFilters() const { return m_filters; }

    QQmlListProperty<UpdaterFilterModel> filters()
    {
        return QQmlListProperty<UpdaterFilterModel>(
            this,
            this,
            &UpdaterPresetModel::appendFilter,
            &UpdaterPresetModel::countFilters,
            &UpdaterPresetModel::filterAt,
            &UpdaterPresetModel::clearFilters
            );
    }

    void addFilter(UpdaterFilterModel* filter) {
        if (!m_filters.contains(filter)) {
            m_filters.append(filter);
            emit filtersChanged();
        }
    }

    Q_INVOKABLE void addFilterWithValues(const QString &field, const QString &pattern) {
        auto* filter = new UpdaterFilterModel(this);
        filter->setField(field);
        filter->setPattern(pattern);
        addFilter(filter);
    }

    void removeFilter(UpdaterFilterModel* filter) {
        if (m_filters.removeOne(filter)) {
            filter->deleteLater();
            emit filtersChanged();
        }
    }

    Q_INVOKABLE void removeFilter(int index) {
        if (index < 0 || index >= m_filters.count())
            return;

        auto* filter = m_filters.takeAt(index);
        filter->deleteLater();
        emit filtersChanged();
    }

    bool isSystemPreset() const { return m_isSytemPreset; }
    void setIsSystemPreset(bool v) { if (m_isSytemPreset != v) { m_isSytemPreset = v; emit isSystemPresetChanged(); } }

signals:
    void nameChanged();
    void typeChanged();
    void urlChanged();
    void versionFieldChanged();
    void versionPatternChanged();
    void downloadFieldChanged();
    void downloadPatternChanged();
    void dateFieldChanged();
    void filtersChanged();
    void isSystemPresetChanged();

private:
    QString m_name;
    QString m_type;
    QString m_url;
    QString m_versionField;
    QString m_versionPattern;
    QString m_downloadField;
    QString m_downloadPattern;
    QString m_dateField;
    QList<UpdaterFilterModel*> m_filters;
    bool m_isSytemPreset = false;

    static void appendFilter(QQmlListProperty<UpdaterFilterModel>* list, UpdaterFilterModel* filter) {
        auto* preset = qobject_cast<UpdaterPresetModel*>(list->object);
        if (preset && filter) preset->addFilter(filter);
    }

    static qsizetype countFilters(QQmlListProperty<UpdaterFilterModel>* list) {
        auto* preset = qobject_cast<UpdaterPresetModel*>(list->object);
        return preset ? preset->m_filters.count() : 0;
    }

    static UpdaterFilterModel* filterAt(QQmlListProperty<UpdaterFilterModel>* list, qsizetype index) {
        auto* preset = qobject_cast<UpdaterPresetModel*>(list->object);
        return preset ? preset->m_filters.at(index) : nullptr;
    }

    static void clearFilters(QQmlListProperty<UpdaterFilterModel>* list) {
        auto* preset = qobject_cast<UpdaterPresetModel*>(list->object);
        if (preset) {
            preset->m_filters.clear();
            emit preset->filtersChanged();
        }
    }
};

#endif // UPDATERPRESETMODEL_H
