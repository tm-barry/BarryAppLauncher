#include "jsonupdater.h"
#include "managers/errormanager.h"
#include "utils/jsonutil.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QRegularExpression>

JsonUpdater::JsonUpdater(QObject *parent) : IUpdater(parent) {}
JsonUpdater::JsonUpdater(const UpdaterSettings &settings, QObject *parent) : IUpdater(settings, parent) {}

void JsonUpdater::parseData(const QByteArray &data)
{
    m_releases.clear();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        ErrorManager::instance()->reportError("JSON parse error: " + err.errorString());
        return;
    }

    QJsonValue root;
    if (doc.isArray())       root = doc.array();
    else if (doc.isObject()) root = doc.object();
    else {
        ErrorManager::instance()->reportError("Unexpected JSON structure");
        return;
    }

    QRegularExpression downloadRe(m_settings.downloadPattern);
    if (!downloadRe.isValid()) {
        ErrorManager::instance()->reportError("Invalid download regex: " + m_settings.downloadPattern);

    }

    QRegularExpression versionRe(m_settings.versionPattern);
    if (!versionRe.isValid()) {
        ErrorManager::instance()->reportError("Invalid version regex: " + m_settings.versionPattern);

    }

    // Expand releases from the root
    QList<QJsonValue> releaseCandidates =
        root.isArray() ? JsonUtil::getValuesByPath(root, "[*]") : QList<QJsonValue>{ root };

    for (const QJsonValue &releaseVal : releaseCandidates) {
        if (!releaseVal.isObject())
            continue;

        const QJsonObject obj = releaseVal.toObject();

        // Apply filters
        bool include = true;
        for (const UpdaterFilter &f : m_settings.filters) {
            QList<QJsonValue> vals = JsonUtil::getValuesByPath(obj, f.field);
            QString fieldVal;
            if (!vals.isEmpty())
                fieldVal = vals.first().toVariant().toString();

            QRegularExpression re(f.pattern);
            if (!re.isValid() || !re.match(fieldVal).hasMatch()) {
                include = false;
                break;
            }
        }
        if (!include) continue;

        // Extract version
        QString version;
        {
            QList<QJsonValue> vals = JsonUtil::getValuesByPath(obj, m_settings.versionField);
            if (!vals.isEmpty()) {
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
            const QList<QJsonValue> candidates = JsonUtil::getValuesByPath(obj, m_settings.downloadField);
            for (const QJsonValue &v : candidates) {
                if (!v.isString()) continue;
                const QString url = v.toString();
                if (downloadRe.pattern().isEmpty() || downloadRe.match(url).hasMatch()) {
                    download = url;
                    break;
                }
            }
        }

        if (!download.isEmpty()) {
            UpdaterRelease r;
            r.version  = version;
            r.date     = date;
            r.download = download;
            m_releases.append(r);
        }
    }
}
