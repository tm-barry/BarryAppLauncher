#ifndef UPDATEPRESETMANAGER_H
#define UPDATEPRESETMANAGER_H

#include "models/appimagemetadata.h"
#include "models/updaterpresetmodel.h"

#include <QObject>
#include <QQmlListProperty>

class UpdatePresetManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QQmlListProperty<UpdaterPresetModel> presets READ presets NOTIFY presetsChanged)

public:
    static UpdatePresetManager* instance();

    QQmlListProperty<UpdaterPresetModel> presets();
    void addPreset(UpdaterPresetModel* preset);
    void removePreset(UpdaterPresetModel* preset);
    Q_INVOKABLE void removePreset(int index);

    Q_INVOKABLE void applyPreset(const UpdaterPresetModel* preset, const QString url, AppImageMetadata* metadata = nullptr);

private:
    explicit UpdatePresetManager(QObject *parent = nullptr);

    QList<UpdaterPresetModel*> m_presets;

    static void appendPreset(QQmlListProperty<UpdaterPresetModel>* list, UpdaterPresetModel* preset);
    static qsizetype countPresets(QQmlListProperty<UpdaterPresetModel>* list);
    static UpdaterPresetModel* presetAt(QQmlListProperty<UpdaterPresetModel>* list, qsizetype index);
    static void clearPresets(QQmlListProperty<UpdaterPresetModel>* list);

signals:
    void presetsChanged();
};

#endif // UPDATEPRESETMANAGER_H
