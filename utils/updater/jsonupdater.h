#ifndef JSONUPDATER_H
#define JSONUPDATER_H

#include "utils/updater/updaterfactory.h"

#include <QList>
#include <QString>

class JsonUpdater : public IUpdater
{
    Q_OBJECT
public:
    explicit JsonUpdater(QObject *parent = nullptr);
    explicit JsonUpdater(const UpdaterSettings &settings,
                         const QString currentVersion = QString(),
                         const QString currentDate = QString(),
                         QObject *parent = nullptr);

    void parseData(const QByteArray &data) override;
};

#endif // JSONUPDATER_H
