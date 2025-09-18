#ifndef UPDATERFACTORY_H
#define UPDATERFACTORY_H

#pragma once

#include "managers/errormanager.h"
#include "utils/networkutil.h"

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
};

struct UpdaterFilter {
public:
    QString field;
    QString pattern;
};

struct UpdaterSettings {
public:
    QString url = QString();
    QString versionField = QString();
    QString downloadField = QString();
    QString downloadPattern = QString();
    QString dateField = QString();
    QList<UpdaterFilter> filters { };
};

class IUpdater : public QObject
{
    Q_OBJECT
public:
    explicit IUpdater(QObject *parent = nullptr)
        : QObject(parent) {}

    explicit IUpdater(const UpdaterSettings &settings, QObject *parent = nullptr)
        : IUpdater(parent)
    {
        m_settings = settings;
    }

    virtual ~IUpdater() = default;

    const QList<UpdaterRelease>& releases() const { return m_releases; }

    virtual void parseData(const QByteArray &data) = 0;

    void updateSettings(const UpdaterSettings &settings)
    {
        m_settings = settings;
    }

    void fetchUpdatesAsync(const QString &url = QString())
    {
        QString targetUrl = url.isEmpty() ? m_settings.url : url;
        QNetworkRequest req((QUrl(targetUrl)));
        req.setHeader(QNetworkRequest::UserAgentHeader, "BarryAppLauncher");
        QNetworkReply *reply = NetworkUtil::networkManager()->get(req);

        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QByteArray data = reply->readAll();
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

            // Always emit updatesReady, even on failure
            emit updatesReady();
        });
    }

signals:
    void updatesReady();

protected:
    UpdaterSettings m_settings;
    QList<UpdaterRelease> m_releases;
};

class UpdaterFactory
{
public:
    UpdaterFactory();

    static IUpdater* create(const QString &type, const UpdaterSettings &settings);
};

#endif // UPDATERFACTORY_H
