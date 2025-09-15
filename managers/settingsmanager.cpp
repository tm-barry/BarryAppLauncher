#include "settingsmanager.h"
#include "utils/terminalutil.h"
#include "utils/texteditorutil.h"

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

QString SettingsManager::terminal() const {
    QSettings settings;
    QVariant v = settings.value("General/terminal", m_terminalDefault);
    return v.toString().isEmpty() ? m_terminalDefault : v.toString();
}

void SettingsManager::setTerminal(QString value) {
    if (terminal() == value)
        return;

    if(value.isEmpty())
        value = m_terminalDefault;

    QSettings settings;
    settings.setValue("General/terminal", value);
    emit terminalChanged(value);
}

QString SettingsManager::textEditor() const {
    QSettings settings;
    QVariant v = settings.value("General/textEditor", m_textEditorDefault);
    return v.toString().isEmpty() ? m_textEditorDefault : v.toString();
}

void SettingsManager::setTextEditor(QString value) {
    if (textEditor() == value)
        return;

    if(value.isEmpty())
        value = m_textEditorDefault;

    QSettings settings;
    settings.setValue("General/textEditor", value);
    emit textEditorChanged(value);
}

bool SettingsManager::terminalExists(const QString& path)
{
    return TerminalUtil::terminalExists(path);
}

bool SettingsManager::textEditorExists(const QString& path)
{
    return TextEditorUtil::textEditorExists(path);
}

// ----------------- Private -----------------

const QUrl SettingsManager::m_appImageDefaultLocation = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Applications");
const QString SettingsManager::m_terminalDefault = TerminalUtil::detectTerminal();
const QString SettingsManager::m_textEditorDefault = TextEditorUtil::detectTextEditor();


SettingsManager::SettingsManager(QObject *parent)
    : QObject{parent}
{}
