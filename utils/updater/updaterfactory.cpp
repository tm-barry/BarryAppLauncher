#include "updaterfactory.h"
#include "jsonupdater.h"

// ----------------- Public -----------------

UpdaterFactory::UpdaterFactory() {}

IUpdater* UpdaterFactory::create(const QString &type, const UpdaterSettings &settings)
{
    if (type == "static") {
        // TODO - static updater
        return nullptr;
    }
    else if (type == "json") {
        return new JsonUpdater(settings);
    }
    return nullptr;
}
