#ifndef UPDATERFACTORY_H
#define UPDATERFACTORY_H

#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QByteArray>
#include <QString>

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
        QNetworkReply *reply = manager.get(req);

        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            QByteArray data = reply->readAll();
            reply->deleteLater();
            parseData(data);
            emit updatesReady();
        });
    }

signals:
    void updatesReady();

protected:
    QNetworkAccessManager manager;
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
