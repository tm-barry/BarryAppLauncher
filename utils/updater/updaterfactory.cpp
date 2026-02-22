#include "updaterfactory.h"
#include "jsonupdater.h"
#include "staticupdater.h"

// ----------------- Public -----------------

UpdaterFactory::UpdaterFactory() {}

IUpdater* UpdaterFactory::create(const QString &type, const UpdaterSettings &settings)
{
    if (type == "json") {
        return new JsonUpdater(settings);
    }
    else if (type == "static")
    {
        return new StaticUpdater(settings);
    }

    return nullptr;
}

const QList<UpdaterSettingsPreset> UpdaterFactory::getDefaultPresets()
{
    return {
        UpdaterSettingsPreset{
            .name = "GitHub-Latest",
            .settings = UpdaterSettings{
                .type = "json",
                .url = "https://api.github.com/repos/{owner}/{repo}/releases/latest",
                .versionField = "tag_name",
                .downloadField = "assets[*].browser_download_url",
                .downloadPattern = ".*\\.AppImage",
                .dateField = "published_at"
            }
        },
        UpdaterSettingsPreset{
            .name = "GitHub-Prerelease",
            .settings = UpdaterSettings{
                .type = "json",
                .url = "https://api.github.com/repos/{owner}/{repo}/releases",
                .versionField = "tag_name",
                .downloadField = "assets[*].browser_download_url",
                .downloadPattern = ".*\\.AppImage",
                .dateField = "published_at",
                .filters = { UpdaterFilter{ .field = "prerelease", .pattern = "true" } }
            }
        }
    };
}
