#include "updatepresetmanager.h"

#include "appimagemanager.h"

// ----------------- Public -----------------

UpdatePresetManager* UpdatePresetManager::instance() {
    static UpdatePresetManager singleton;
    return &singleton;
}

QQmlListProperty<UpdaterPresetModel> UpdatePresetManager::presets()
{
    return QQmlListProperty<UpdaterPresetModel>(
        this,
        this,
        &appendPreset,
        &countPresets,
        &presetAt,
        &clearPresets
    );
}

void UpdatePresetManager::addPreset(UpdaterPresetModel* preset) {
    if (!m_presets.contains(preset)) {
        m_presets.append(preset);
        emit presetsChanged();
    }
}

void UpdatePresetManager::removePreset(UpdaterPresetModel* preset) {
    if (m_presets.removeOne(preset)) {
        preset->deleteLater();
        emit presetsChanged();
    }
}

Q_INVOKABLE void UpdatePresetManager::removePreset(int index) {
    if (index < 0 || index >= m_presets.count())
        return;

    auto* preset = m_presets.takeAt(index);
    preset->deleteLater();
    emit presetsChanged();
}

Q_INVOKABLE void UpdatePresetManager::applyPreset(const UpdaterPresetModel* preset, const QString url, AppImageMetadata* metadata)
{
    if (!preset) return;
    if (!metadata) {
        metadata = AppImageManager::instance()->appImageMetadata();
        if (!metadata) return;
    }

    metadata->setUpdateType(preset->type());
    metadata->setUpdateUrl(url);
    metadata->setUpdateDownloadField(preset->downloadField());
    metadata->setUpdateDownloadPattern(preset->downloadPattern());
    metadata->setUpdateDateField(preset->dateField());
    metadata->setUpdateVersionField(preset->versionField());
    metadata->setUpdateVersionPattern(preset->versionPattern());

    // Add filters
    metadata->clearUpdateFilters();
    for (const auto& filter : preset->getFilters()) {
        metadata->addUpdateFilterWithValues(filter->field(), filter->pattern());
    }

    // Clear current update version/date
    metadata->setUpdateCurrentDate(QString());
    metadata->setUpdateCurrentVersion(QString());
}

// ----------------- Prvate -----------------

UpdatePresetManager::UpdatePresetManager(QObject *parent)
    : QObject{parent}
{
    auto systemPresets = UpdaterFactory::getDefaultPresets();
    for (const auto& systemPreset : systemPresets) {
        addPreset(UpdaterPresetModel::createFromUtil(systemPreset, true, this));
    }
}

void UpdatePresetManager::appendPreset(QQmlListProperty<UpdaterPresetModel>* list, UpdaterPresetModel* preset) {
    auto* manager = qobject_cast<UpdatePresetManager*>(list->object);
    if (manager && preset) manager->addPreset(preset);
}

qsizetype UpdatePresetManager::countPresets(QQmlListProperty<UpdaterPresetModel>* list) {
    auto* manager = qobject_cast<UpdatePresetManager*>(list->object);
    return manager ? manager->m_presets.count() : 0;
}

UpdaterPresetModel* UpdatePresetManager::presetAt(QQmlListProperty<UpdaterPresetModel>* list, qsizetype index) {
    auto* manager = qobject_cast<UpdatePresetManager*>(list->object);
    return manager ? manager->m_presets.at(index) : nullptr;
}

void UpdatePresetManager::clearPresets(QQmlListProperty<UpdaterPresetModel>* list) {
    auto* manager = qobject_cast<UpdatePresetManager*>(list->object);
    if (manager) {
        manager->m_presets.clear();
        emit manager->presetsChanged();
    }
}
