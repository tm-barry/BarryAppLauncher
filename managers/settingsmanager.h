#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#pragma once

#include <QObject>

class SettingsManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QUrl appImageDefaultLocation READ appImageDefaultLocation WRITE setAppImageDefaultLocation NOTIFY appImageDefaultLocationChanged);
    Q_PROPERTY(AppImageFileOperation appImageFileOperation READ appImageFileOperation WRITE setAppImageFileOperation NOTIFY appImageFileOperationChanged);
    Q_PROPERTY(QString terminal READ terminal WRITE setTerminal NOTIFY terminalChanged);
public:
    static SettingsManager* instance();

    enum AppImageFileOperation {
        Move,
        Copy
    };
    Q_ENUM(AppImageFileOperation);

    QUrl appImageDefaultLocation() const;
    void setAppImageDefaultLocation(QUrl value);

    AppImageFileOperation appImageFileOperation() const;
    void setAppImageFileOperation(AppImageFileOperation value);

    QString terminal() const;
    void setTerminal(QString value);

    Q_INVOKABLE bool terminalExists(const QString& path);

private:
    explicit SettingsManager(QObject *parent = nullptr);

    static const QUrl m_appImageDefaultLocation;
    static const QString m_terminalDefault;

signals:
    void appImageDefaultLocationChanged(QUrl newValue);
    void appImageFileOperationChanged(SettingsManager::AppImageFileOperation newValue);
    void terminalChanged(QString newValue);
};

#endif // SETTINGSMANAGER_H
