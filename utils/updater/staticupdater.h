#ifndef STATICUPDATER_H
#define STATICUPDATER_H

#include "utils/updater/updaterfactory.h"

#include <QList>
#include <QString>

class StaticUpdater : public IUpdater
{
    Q_OBJECT
public:
    explicit StaticUpdater(QObject *parent = nullptr);
    explicit StaticUpdater(const UpdaterSettings &settings, QObject *parent = nullptr);

    void parseData(const QByteArray &data) override;
};

#endif // STATICUPDATER_H
