#include "settingsmanager.h"
#include "utils/terminalutil.h"
#include "utils/texteditorutil.h"

#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

// ----------------- Public -----------------

SettingsManager* SettingsManager::instance() {
    static SettingsManager singleton;
    return &singleton;
}

QUrl SettingsManager::appImageDefaultLocation() const {
    QVariant v = m_settings.value(SettingsKeys::General::AppImageDefaultLocation, m_appImageDefaultLocation.toString());
    return QUrl(v.toString());
}

void SettingsManager::setAppImageDefaultLocation(QUrl value) {
    if (appImageDefaultLocation() == value)
        return;

    m_settings.setValue(SettingsKeys::General::AppImageDefaultLocation, value.toString());
    emit appImageDefaultLocationChanged(value);
}

bool SettingsManager::appListCompactView() const {
    return m_settings.value(SettingsKeys::General::AppListCompactView, false).toBool();
}

void SettingsManager::setAppListCompactView(bool value) {
    if (appListCompactView() == value)
        return;

    m_settings.setValue(SettingsKeys::General::AppListCompactView, value);
    emit appListCompactViewChanged(value);
}

SettingsManager::AppImageFileOperation SettingsManager::appImageFileOperation() const {
    return static_cast<AppImageFileOperation>(
        m_settings.value(
            SettingsKeys::General::AppImageFileOperation,
            static_cast<int>(AppImageFileOperation::Move)
        ).value<int>()
    );
}

void SettingsManager::setAppImageFileOperation(AppImageFileOperation value) {
    if (appImageFileOperation() == value)
        return;

    m_settings.setValue(SettingsKeys::General::AppImageFileOperation, static_cast<int>(value));
    emit appImageFileOperationChanged(value);
}

QString SettingsManager::terminal() const {
    QVariant v = m_settings.value(SettingsKeys::General::Terminal, m_terminalDefault);
    return v.toString().isEmpty() ? m_terminalDefault : v.toString();
}

void SettingsManager::setTerminal(QString value) {
    if (terminal() == value)
        return;

    if(value.isEmpty())
        value = m_terminalDefault;

    m_settings.setValue(SettingsKeys::General::Terminal, value);
    emit terminalChanged(value);
}

QString SettingsManager::textEditor() const {
    QVariant v = m_settings.value(SettingsKeys::General::TextEditor, m_textEditorDefault);
    return v.toString().isEmpty() ? m_textEditorDefault : v.toString();
}

void SettingsManager::setTextEditor(QString value) {
    if (textEditor() == value)
        return;

    if(value.isEmpty())
        value = m_textEditorDefault;

    m_settings.setValue(SettingsKeys::General::TextEditor, value);
    emit textEditorChanged(value);
}

bool SettingsManager::keepBackup() const {
    QVariant v = m_settings.value(SettingsKeys::General::KeepBackup, false);
    return v.toBool();
}

void SettingsManager::setKeepBackup(bool value) {
    if (keepBackup() == value)
        return;

    m_settings.setValue(SettingsKeys::General::KeepBackup, value);
    emit keepBackupChanged(value);
}

int SettingsManager::updateConcurrency() const {
    return m_settings.value(SettingsKeys::Update::UpdateConcurrency, 3).toInt();
}

void SettingsManager::setUpdateConcurrency(int value) {
    if (updateConcurrency() == value)
        return;

    m_settings.setValue(SettingsKeys::Update::UpdateConcurrency, value);
    emit updateConcurrencyChanged(value);
}

bool SettingsManager::terminalExists(const QString& path)
{
    return TerminalUtil::terminalExists(path);
}

bool SettingsManager::textEditorExists(const QString& path)
{
    return TextEditorUtil::textEditorExists(path);
}

void SettingsManager::saveUpdateHeadersJson(const QJsonArray &headers) {
    QJsonDocument doc(headers);
    m_settings.setValue(SettingsKeys::Update::Headers, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void SettingsManager::saveUpdateHeaders(const QList<UpdateHeader> &headers) {
    QJsonArray array;
    for (const auto &h : headers) {
        QJsonObject obj;
        obj["website"] = h.website;
        obj["header"] = h.header;
        obj["value"] = h.value;
        array.append(obj);
    }
    saveUpdateHeadersJson(array);
}

QJsonArray SettingsManager::getUpdateHeadersJson() const {
    QString json = m_settings.value(SettingsKeys::Update::Headers, "[]").toString();
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    return doc.isArray() ? doc.array() : QJsonArray();
}

QList<UpdateHeader> SettingsManager::getUpdateHeaders() const {
    const QJsonArray headers = getUpdateHeadersJson();
    QList<UpdateHeader> list;
    list.reserve(headers.size());
    for (const auto &v : headers) {
        QJsonObject obj = v.toObject();
        list.append(UpdateHeader{
            obj["website"].toString(),
            obj["header"].toString(),
            obj["value"].toString()
        });
    }
    return list;
}

// ----------------- Private -----------------

const QUrl SettingsManager::m_appImageDefaultLocation = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Applications");
const QString SettingsManager::m_terminalDefault = TerminalUtil::detectTerminal();
const QString SettingsManager::m_textEditorDefault = TextEditorUtil::detectTextEditor();


SettingsManager::SettingsManager(QObject *parent)
    : QObject{parent},
    m_settings(
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            + "/BarryAppLauncher.conf",
        QSettings::IniFormat)
{
    const QString newFile = m_settings.fileName();

    const QString oldFile =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + "/tm-barry/BarryAppLauncher.conf";

    QDir().mkpath(QFileInfo(newFile).absolutePath());

    if (QFile::exists(oldFile) && !QFile::exists(newFile)) {
        if (QFile::rename(oldFile, newFile)) {
            m_settings.sync();
        }
    }
}
