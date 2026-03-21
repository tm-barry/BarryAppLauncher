#ifndef UPDATERFACTORY_H
#define UPDATERFACTORY_H

#pragma once

#include "managers/errormanager.h"
#include "managers/settingsmanager.h"
#include "utils/networkutil.h"
#include "utils/stringutil.h"
#include "utils/versionutil.h"

#include <QObject>
#include <QNetworkReply>
#include <QEventLoop>
#include <QByteArray>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>

struct UpdaterRelease {
public:
    QString download = QString();
    QString version = QString();
    QString date = QString();
    bool isNew = false;
    bool isLatest = false;

    bool operator==(const UpdaterRelease &other) const {
        return download == other.download
               && version == other.version
               && date == other.date
               && isNew == other.isNew
               && isLatest == other.isLatest;
    }
};

struct UpdaterFilter {
public:
    QString field;
    QString pattern;
};

struct UpdaterSettings {
public:
    QString type = QString();
    QString url = QString();
    QString versionField = QString();
    QString versionPattern = QString();
    QString downloadField = QString();
    QString downloadPattern = QString();
    QString dateField = QString();
    QList<UpdaterFilter> filters;
};

struct UpdaterSettingsPreset {
public:
    QString name = QString();
    UpdaterSettings settings;
};

class IUpdater : public QObject
{
    Q_OBJECT
public:
    explicit IUpdater(QObject *parent = nullptr)
        : QObject(parent) {}

    explicit IUpdater(const UpdaterSettings &settings,
                      const QString currentVersion = QString(),
                      const QString currentDate = QString(),
                      QObject *parent = nullptr)
        : IUpdater(parent)
    {
        m_settings = settings;
        m_currentVersion = currentVersion;
        m_currentDate = currentDate;
    }

    virtual ~IUpdater() = default;

    const QList<UpdaterRelease>& releases() const { return m_releases; }

    virtual void parseData(const QByteArray &data) = 0;

    void updateSettings(
        const UpdaterSettings &settings,
        const QString currentVersion = QString(),
        const QString currentDate = QString())
    {
        m_settings = settings;
    }

    void fetchUpdatesAsync(const QString &url = QString())
    {
        QString targetUrl = url.isEmpty() ? m_settings.url : url;
        QNetworkRequest req((QUrl(targetUrl)));
        req.setHeader(QNetworkRequest::UserAgentHeader, "BarryAppLauncher");

        // Apply custom user update headers
        auto updateHeaders = SettingsManager::instance()->getUpdateHeaders();
        for (const auto &header : updateHeaders) {
            if (header.header.isEmpty() || header.value.isEmpty())
                continue;

            if (header.website.isEmpty() || targetUrl.contains(header.website)) {
                req.setRawHeader(header.header.toUtf8(), header.value.toUtf8());
            }
        }

        QNetworkReply *reply = nullptr;

        if (m_headersOnly) {
            // HEAD request = fetch headers only
            reply = NetworkUtil::networkManager()->head(req);
        } else {
            // GET request = fetch full body
            reply = NetworkUtil::networkManager()->get(req);
        }

        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QByteArray data;

            if (m_headersOnly) {
                QJsonObject jsonHeaders;

                const QList<QNetworkReply::RawHeaderPair> headers = reply->rawHeaderPairs();
                for (const auto &header : headers) {
                    jsonHeaders[QString(header.first)] = QString(header.second);
                }

                // Add final URL and HTTP status
                jsonHeaders["url"] = reply->url().toString();
                jsonHeaders["status"] = status;

                data = QJsonDocument(jsonHeaders).toJson(QJsonDocument::Compact);
            }
            else {
                data = reply->readAll();
            }

            reply->deleteLater();

            if (status >= 200 && status < 300) {
                // Success: parse JSON normally
                parseData(data);
            } else {
                // Generic error handling
                QString errorMsg = QString("Request failed with status %1").arg(status);

                // Attempt to extract a JSON error message if available
                QJsonDocument json = QJsonDocument::fromJson(data);
                if (!json.isNull() && json.isObject()) {
                    QJsonObject obj = json.object();
                    if (obj.contains("message")) {
                        errorMsg += ": " + obj["message"].toString();
                    }
                }

                ErrorManager::instance()->reportError("API Error (" + QString::number(status) + "): " + errorMsg);
            }

            // Mark releases
            markReleases(m_releases, m_currentVersion, m_currentDate);

            // Always emit updatesReady, even on failure
            emit updatesReady();
        });
    }

    void markReleases(QList<UpdaterRelease> &releases, const QString &currentVersion, const QString &currentDate)
    {
        if (releases.isEmpty())
            return;
        // Find latest release
        auto latestIt = std::max_element(releases.begin(), releases.end(),
                                         [](const UpdaterRelease &a, const UpdaterRelease &b) {
                                             // Compare by version first, then by date if versions are equal
                                             int cmp = VersionUtil::compareVersions(a.version, b.version);
                                             if (cmp != 0) return cmp < 0;
                                             return StringUtil::parseDateTime(a.date) < StringUtil::parseDateTime(b.date);
                                         });

        // Mark latest release
        if (latestIt != releases.end())
            latestIt->isLatest = true;

        // Mark new releases
        for (auto &r : releases) {
            r.isNew = isNewRelease(r, currentVersion, currentDate);
        }
    }

    static bool isNewRelease(const UpdaterRelease &release,
                             const QString currentVersion = QString(),
                             const QString currentDate = QString())
    {
        return (currentVersion.isEmpty() || VersionUtil::compareVersions(release.version, currentVersion) == 1)
        && (currentDate.isEmpty() || StringUtil::parseDateTime(release.date) > StringUtil::parseDateTime(currentDate));
    }

signals:
    void updatesReady();

protected:
    UpdaterSettings m_settings;
    QString m_currentVersion;
    QString m_currentDate;
    bool m_headersOnly = false;
    QList<UpdaterRelease> m_releases;
};

class UpdaterFactory
{
public:
    UpdaterFactory();

    static IUpdater* create(const QString &type,
                            const UpdaterSettings &settings,
                            const QString currentVersion = QString(),
                            const QString currentDate = QString());
    static const QList<UpdaterSettingsPreset> getDefaultPresets();
};

#endif // UPDATERFACTORY_H
