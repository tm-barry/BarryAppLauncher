#include "settingsmanager.h"

#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

// ----------------- Public -----------------

SettingsManager* SettingsManager::instance() {
    static SettingsManager singleton;
    return &singleton;
}

QUrl SettingsManager::appImageDefaultLocation() const {
    QSettings settings;
    QVariant v = settings.value("General/appImageDefaultLocation", m_appImageDefaultLocation.toString());
    return QUrl(v.toString());
}

void SettingsManager::setAppImageDefaultLocation(QUrl value) {
    if (appImageDefaultLocation() == value)
        return;
    QSettings settings;
    settings.setValue("General/appImageDefaultLocation", value.toString());
    emit appImageDefaultLocationChanged(value);
}

SettingsManager::AppImageFileOperation SettingsManager::appImageFileOperation() const {
    QSettings settings;
    return static_cast<AppImageFileOperation>(settings.value("General/appImageFileOperation", static_cast<int>(AppImageFileOperation::Move)).value<int>());
}

void SettingsManager::setAppImageFileOperation(AppImageFileOperation value) {
    if (appImageFileOperation() == value)
        return;
    QSettings settings;
    settings.setValue("General/appImageFileOperation", static_cast<int>(value));
    emit appImageFileOperationChanged(value);
}

// ----------------- Private -----------------

const QUrl SettingsManager::m_appImageDefaultLocation = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Applications");

SettingsManager::SettingsManager(QObject *parent)
    : QObject{parent}
{}
