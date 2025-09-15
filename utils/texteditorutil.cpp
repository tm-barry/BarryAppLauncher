#include "texteditorutil.h"
#include "managers/errormanager.h"
#include "managers/settingsmanager.h"


#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QDebug>
#include <QProcessEnvironment>
#include <QString>
#include <QRegularExpression>

// ----------------- Public -----------------

TextEditorUtil::TextEditorUtil() {}

const QString TextEditorUtil::detectTextEditor()
{
    QString editor = detectTextEditorByEnv();

    if(editor.isEmpty())
        editor = detectTextEditorByMime();

    if(editor.isEmpty())
        editor = detectTextEditorByPath();

    if(editor.isEmpty())
        editor = "nano";

    return editor;
}

const bool TextEditorUtil::textEditorExists(const QString& path)
{
    if (path.isEmpty())
        return false;

    if (path.endsWith(".desktop")) {
        QStringList searchPaths = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
        for (const QString &dir : searchPaths) {
            if (QFile::exists(dir + "/" + path))
                return true;
        }
        return false;
    }

    return !QStandardPaths::findExecutable(path).isEmpty();
}

const bool TextEditorUtil::launchInTextEditor(const QString &path)
{
    if (path.isEmpty())
        return false;

    QString editorCmd = SettingsManager::instance()->textEditor();
    if (editorCmd.isEmpty())
        editorCmd = detectTextEditor();

    if(editorCmd.endsWith(".desktop"))
        editorCmd = getExecFromDesktopFile(editorCmd);

    if(editorCmd.isEmpty())
        return false;

    QStringList parts = editorCmd.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;

    QString program = parts.takeFirst();
    QStringList args = parts;

    args.append(path);

    bool started = QProcess::startDetached(program, args);
    if (!started) {
        QString argsStr = args.join(" ");
        QString message = QString("Failed to launch editor: %1 %2").arg(program, argsStr);
        ErrorManager::instance()->reportError(message);
    }

    return started;
}

// ----------------- Private -----------------

const QString TextEditorUtil::detectTextEditorByEnv()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString editor = env.value("EDITOR");
    if (editor.isEmpty())
        editor = env.value("VISUAL");

    if(editor.endsWith(".desktop"))
        editor = getExecFromDesktopFile(editor);

    return editor;
}

const QString TextEditorUtil::detectTextEditorByMime()
{
    QProcess proc;
    proc.start("xdg-mime", {"query", "default", "text/plain"});
    proc.waitForFinished();
    QString desktopFile = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();

    if (desktopFile.isEmpty())
        return {};

    return getExecFromDesktopFile(desktopFile);
}

const QString TextEditorUtil::detectTextEditorByPath()
{
    QStringList textEditors = { "gedit", "kate", "xed", "mousepad", "code", "subl", "nano", "vim" };

    for (const QString &c : textEditors) {
        QString path = QStandardPaths::findExecutable(c);
        if (!path.isEmpty())
            return c;
    }
    return QString();
}

const QString TextEditorUtil::getExecFromDesktopFile(const QString& filePath)
{
    QStringList searchPaths = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    QString desktopPath;
    for (const QString &searchPath : searchPaths) {
        QString candidate = searchPath + "/" + filePath;
        if (QFile::exists(candidate)) {
            desktopPath = candidate;
            break;
        }
    }

    if (desktopPath.isEmpty())
        return {};

    QSettings settings(desktopPath, QSettings::IniFormat);
    settings.beginGroup("Desktop Entry");
    QString exec = settings.value("Exec").toString();
    settings.endGroup();

    if (exec.isEmpty())
        return {};

    exec.replace("%f", "")
        .replace("%F", "")
        .replace("%u", "")
        .replace("%U", "")
        .replace("%i", "")
        .replace("%c", "")
        .replace("%k", "")
        .replace("%%", "%");

    return exec.trimmed();
}
