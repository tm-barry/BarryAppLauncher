#include "appimagemetadata.h"

// ----------------- Public -----------------

AppImageMetadata::AppImageMetadata(QObject* parent)
    : QObject(parent)
    , m_type(0)
    , m_integration(None)
{}

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

QString AppImageMetadata::checksum() const { return m_checksum; }
void AppImageMetadata::setChecksum(const QString& value) {
    if (m_checksum != value) {
        m_checksum = value;
        emit checksumChanged();
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

bool AppImageMetadata::executable() const { return m_executable; }
void AppImageMetadata::setExecutable(bool value) {
    if (m_executable != value) {
        m_executable = value;
        emit executableChanged();
    }
}

QString AppImageMetadata::updateType() const { return m_updateType; }
void AppImageMetadata::setUpdateType(const QString& value) {
    if (m_updateType != value) {
        m_updateType = value;
        setUpdateDirty(true);
        emit updateTypeChanged();
    }
}

QString AppImageMetadata::updateUrl() const { return m_updateUrl; }
void AppImageMetadata::setUpdateUrl(const QString& value) {
    if (m_updateUrl != value) {
        m_updateUrl = value;
        setUpdateDirty(true);
        emit updateUrlChanged();
    }
}

QString AppImageMetadata::updateDownloadField() const { return m_updateDownloadField; }
void AppImageMetadata::setUpdateDownloadField(const QString& value) {
    if (m_updateDownloadField != value) {
        m_updateDownloadField = value;
        setUpdateDirty(true);
        emit updateDownloadFieldChanged();
    }
}

QString AppImageMetadata::updateDownloadPattern() const { return m_updateDownloadPattern; }
void AppImageMetadata::setUpdateDownloadPattern(const QString& value) {
    if (m_updateDownloadPattern != value) {
        m_updateDownloadPattern = value;
        setUpdateDirty(true);
        emit updateDownloadPatternChanged();
    }
}

QString AppImageMetadata::updateDateField() const { return m_updateDateField; }
void AppImageMetadata::setUpdateDateField(const QString& value) {
    if (m_updateDateField != value) {
        m_updateDateField = value;
        setUpdateDirty(true);
        emit updateDateFieldChanged();
    }
}

QString AppImageMetadata::updateVersionField() const { return m_updateVersionField; }
void AppImageMetadata::setUpdateVersionField(const QString& value) {
    if (m_updateVersionField != value) {
        m_updateVersionField = value;
        setUpdateDirty(true);
        emit updateVersionFieldChanged();
    }
}


QQmlListProperty<UpdaterFilterModel> AppImageMetadata::updateFilters()
{
    return QQmlListProperty<UpdaterFilterModel>(
        this,
        this,
        &AppImageMetadata::appendUpdateFilter,
        &AppImageMetadata::updateFiltersCount,
        &AppImageMetadata::updateFilterAt,
        &AppImageMetadata::clearUpdateFilters
        );
}

void AppImageMetadata::addUpdateFilter(UpdaterFilterModel* filter)
{
    m_updateFilters.append(filter);

    connect(filter, &UpdaterFilterModel::fieldChanged,
            this, &AppImageMetadata::onUpdateFilterPropertiesChanged, Qt::UniqueConnection);
    connect(filter, &UpdaterFilterModel::patternChanged,
            this, &AppImageMetadata::onUpdateFilterPropertiesChanged, Qt::UniqueConnection);

    onUpdateFilterChanged();
}

Q_INVOKABLE void AppImageMetadata::addUpdateFilterWithValues(const QString &field, const QString &pattern) {
    auto* filter = new UpdaterFilterModel(this);
    filter->setField(field);
    filter->setPattern(pattern);
    addUpdateFilter(filter);
}

Q_INVOKABLE void AppImageMetadata::removeUpdateFilter(int index) {
    if (index < 0 || index >= m_updateFilters.count())
        return;

    auto* filter = m_updateFilters.takeAt(index);
    filter->deleteLater();
    onUpdateFilterChanged();
}

bool AppImageMetadata::updateDirty() const { return m_updateDirty; }
void AppImageMetadata::setUpdateDirty(bool value) {
    if (m_updateDirty != value) {
        m_updateDirty = value;
        emit updateDirtyChanged();
    }
}

QQmlListProperty<UpdaterReleaseModel> AppImageMetadata::updaterReleases() {
    return QQmlListProperty<UpdaterReleaseModel>(
        this,
        this,
        &AppImageMetadata::updaterReleasesCount,
        &AppImageMetadata::updaterReleasesAt
        );
}

void AppImageMetadata::addUpdaterRelease(UpdaterReleaseModel *release) {
    release->setParent(this);
    m_updaterReleases.append(release);
    emit updaterReleasesChanged();
}

void AppImageMetadata::clearUpdaterReleases()
{
    qDeleteAll(m_updaterReleases);
    m_updaterReleases.clear();
    emit updaterReleasesChanged();
}

// ----------------- Prvate -----------------

void AppImageMetadata::onUpdateFilterChanged()
{
    emit updateFiltersChanged();
    setUpdateDirty(true);
}

void AppImageMetadata::onUpdateFilterPropertiesChanged()
{
    setUpdateDirty(true);
}

void AppImageMetadata::appendUpdateFilter(QQmlListProperty<UpdaterFilterModel>* list, UpdaterFilterModel* filter) {
    AppImageMetadata* metadata = static_cast<AppImageMetadata*>(list->object);
    if (!metadata || !filter) return;
    metadata->addUpdateFilter(filter);
}

qsizetype AppImageMetadata::updateFiltersCount(QQmlListProperty<UpdaterFilterModel>* list)
{
    AppImageMetadata* metadata = static_cast<AppImageMetadata*>(list->object);
    return metadata ? metadata->m_updateFilters.count() : 0;
}

UpdaterFilterModel* AppImageMetadata::updateFilterAt(QQmlListProperty<UpdaterFilterModel>* list, qsizetype index)
{
    AppImageMetadata* metadata = static_cast<AppImageMetadata*>(list->object);
    if (!metadata || index < 0 || index >= metadata->m_updateFilters.count())
        return nullptr;
    return metadata->m_updateFilters.at(index);
}

void AppImageMetadata::clearUpdateFilters(QQmlListProperty<UpdaterFilterModel>* list) {
    AppImageMetadata* metadata = static_cast<AppImageMetadata*>(list->object);
    if (!metadata) return;

    for (auto* filter : metadata->m_updateFilters)
    {
        filter->disconnect(metadata);
        filter->deleteLater();
    }

    metadata->m_updateFilters.clear();
    metadata->onUpdateFilterChanged();
}

qsizetype AppImageMetadata::updaterReleasesCount(QQmlListProperty<UpdaterReleaseModel> *list)
{
    AppImageMetadata* metadata = static_cast<AppImageMetadata*>(list->data);
    return metadata ? metadata->m_updaterReleases.size() : 0;
}

UpdaterReleaseModel* AppImageMetadata::updaterReleasesAt(QQmlListProperty<UpdaterReleaseModel> *list, qsizetype index)
{
    AppImageMetadata* metadata = static_cast<AppImageMetadata*>(list->data);
    if (!metadata || index < 0 || index >= metadata->m_updaterReleases.size())
        return nullptr;
    return metadata->m_updaterReleases.at(index);
}
