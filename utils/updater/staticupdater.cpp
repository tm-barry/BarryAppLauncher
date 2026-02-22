#include "staticupdater.h"
#include "utils/jsonutil.h"

StaticUpdater::StaticUpdater(QObject *parent) : IUpdater(parent) { m_headersOnly = true; }
StaticUpdater::StaticUpdater(const UpdaterSettings &settings, QObject *parent) : IUpdater(settings, parent) { m_headersOnly = true; }

void StaticUpdater::parseData(const QByteArray &data)
{
    m_releases.clear();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        ErrorManager::instance()->reportError("Static headers parse error: " + err.errorString());
        return;
    }

    QJsonValue root;
    if (doc.isArray())       root = doc.array();
    else if (doc.isObject()) root = doc.object();
    else {
        ErrorManager::instance()->reportError("Unexpected static header structure");
        return;
    }

    QRegularExpression versionRe(m_settings.versionPattern);
    if (!versionRe.isValid()) {
        ErrorManager::instance()->reportError("Invalid version regex: " + m_settings.versionPattern);

    }

    const QJsonObject obj = root.toObject();

    // Extract version
    QString version;
    {
        QList<QJsonValue> vals = JsonUtil::getValuesByPath(obj, m_settings.versionField);
        if (!vals.isEmpty())
        {
            version = vals.first().toVariant().toString();
            if (!versionRe.pattern().isEmpty()) {
                QRegularExpressionMatch match = versionRe.match(version);
                if (match.hasMatch()) {
                    version = match.captured(1).isEmpty() ? match.captured(0) : match.captured(1);
                }
            }
        }
    }

    // Extract date
    QString date;
    {
        QList<QJsonValue> vals = JsonUtil::getValuesByPath(obj, m_settings.dateField);
        if (!vals.isEmpty())
            date = vals.first().toVariant().toString();
    }

    // Extract download url
    QString download;
    {
        QList<QJsonValue> vals = JsonUtil::getValuesByPath(obj, "url");
        if (!vals.isEmpty())
            download = vals.first().toVariant().toString();
    }

    if (!download.isEmpty()) {
        UpdaterRelease r;
        r.version  = version;
        r.date     = date;
        r.download = download;
        m_releases.append(r);
    }
}
