#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

struct UpdateHeader {
public:
    QString website = QString();
    QString header = QString();
    QString value = QString();
};

class SettingsManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QUrl appImageDefaultLocation READ appImageDefaultLocation WRITE setAppImageDefaultLocation NOTIFY appImageDefaultLocationChanged);
    Q_PROPERTY(bool appListCompactView READ appListCompactView WRITE setAppListCompactView NOTIFY appListCompactViewChanged);
    Q_PROPERTY(AppImageFileOperation appImageFileOperation READ appImageFileOperation WRITE setAppImageFileOperation NOTIFY appImageFileOperationChanged);
    Q_PROPERTY(QString terminal READ terminal WRITE setTerminal NOTIFY terminalChanged);
    Q_PROPERTY(QString textEditor READ textEditor WRITE setTextEditor NOTIFY textEditorChanged);
    Q_PROPERTY(bool keepBackup READ keepBackup WRITE setKeepBackup NOTIFY keepBackupChanged);
    Q_PROPERTY(int updateConcurrency READ updateConcurrency WRITE setUpdateConcurrency NOTIFY updateConcurrencyChanged);

public:
    static SettingsManager* instance();

    enum AppImageFileOperation {
        Move,
        Copy
    };
    Q_ENUM(AppImageFileOperation);

    QUrl appImageDefaultLocation() const;
    void setAppImageDefaultLocation(QUrl value);

    bool appListCompactView() const;
    void setAppListCompactView(bool value);

    AppImageFileOperation appImageFileOperation() const;
    void setAppImageFileOperation(AppImageFileOperation value);

    QString terminal() const;
    void setTerminal(QString value);

    QString textEditor() const;
    void setTextEditor(QString value);

    bool keepBackup() const;
    void setKeepBackup(bool value);

    int updateConcurrency() const;
    void setUpdateConcurrency(int value);

    Q_INVOKABLE bool terminalExists(const QString& path);
    Q_INVOKABLE bool textEditorExists(const QString& path);

    Q_INVOKABLE void saveUpdateHeadersJson(const QJsonArray &headers);
    void saveUpdateHeaders(const QList<UpdateHeader> &headers);
    Q_INVOKABLE QJsonArray getUpdateHeadersJson() const;
    QList<UpdateHeader> getUpdateHeaders() const;

private:
    explicit SettingsManager(QObject *parent = nullptr);

    static const QUrl m_appImageDefaultLocation;
    static const QString m_terminalDefault;
    static const QString m_textEditorDefault;

signals:
    void appImageDefaultLocationChanged(QUrl newValue);
    void appListCompactViewChanged(bool newValue);
    void appImageFileOperationChanged(SettingsManager::AppImageFileOperation newValue);
    void terminalChanged(QString newValue);
    void textEditorChanged(QString newValue);
    void keepBackupChanged(bool newValue);
    void updateConcurrencyChanged(int newValue);
};

#endif // SETTINGSMANAGER_H
