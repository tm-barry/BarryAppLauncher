#include "jsonupdater.h"
#include "managers/errormanager.h"

#include <QNetworkAccessManager>
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

    // Expand releases from the root
    QList<QJsonValue> releaseCandidates =
        root.isArray() ? getValuesByPath(root, "[*]") : QList<QJsonValue>{ root };

    for (const QJsonValue &releaseVal : releaseCandidates) {
        if (!releaseVal.isObject())
            continue;

        const QJsonObject obj = releaseVal.toObject();

        // Apply filters
        bool include = true;
        for (const UpdaterFilter &f : m_settings.filters) {
            QList<QJsonValue> vals = getValuesByPath(obj, f.field);
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
            QList<QJsonValue> vals = getValuesByPath(obj, m_settings.versionField);
            if (!vals.isEmpty())
                version = vals.first().toVariant().toString();
        }

        // Extract date
        QString date;
        {
            QList<QJsonValue> vals = getValuesByPath(obj, m_settings.dateField);
            if (!vals.isEmpty())
                date = vals.first().toVariant().toString();
        }

        // Extract download url
        QString download;
        {
            const QList<QJsonValue> candidates = getValuesByPath(obj, m_settings.downloadField);
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

QList<QJsonValue> JsonUpdater::getValuesByPath(const QJsonValue &root, const QString &path) {
    QList<QJsonValue> current { root };

    if (path.isEmpty())
        return { root };

    QStringList segments = path.split('.', Qt::SkipEmptyParts);
    QRegularExpression indexRe(R"(^(.*)\[(\d+)\]$)");

    for (QString &seg : segments) {
        seg = seg.trimmed();
        QList<QJsonValue> next;

        bool isArrayWildcard = seg.endsWith("[*]");
        int arrayIndex = -1;
        QString key = seg;

        // Check for numeric index like "assets[0]"
        QRegularExpressionMatch match = indexRe.match(seg);
        if (match.hasMatch()) {
            key = match.captured(1);
            arrayIndex = match.captured(2).toInt();
        } else if (isArrayWildcard) {
            key = seg.left(seg.length() - 3);
        }

        for (const QJsonValue &val : current) {
            QJsonValue child;

            // Object access
            if (val.isObject() && !key.isEmpty()) {
                const QJsonObject obj = val.toObject();
                child = obj.value(key);
            }

            // Array access at root or intermediate
            else if (val.isArray() && key.isEmpty()) {
                child = val;
            }

            // Wildcard [*]
            if (isArrayWildcard && child.isArray()) {
                for (const QJsonValue &arrVal : child.toArray())
                    next.append(arrVal);
            }
            // Numeric index [n]
            else if (arrayIndex >= 0 && child.isArray()) {
                QJsonArray arr = child.toArray();
                if (arrayIndex < arr.size())
                {
                    next.append(arr[arrayIndex]);
                }
                else
                {
                    QString message = QString("Index %1 out of range for key %2").arg(arrayIndex).arg(key);
                    ErrorManager::instance()->reportError(message);
                }
            }
            // Normal key access
            else if (arrayIndex < 0 && !isArrayWildcard) {
                if (!child.isUndefined())
                    next.append(child);
            }
        }

        current = next;
        if (current.isEmpty()) break;
    }

    return current;
}
