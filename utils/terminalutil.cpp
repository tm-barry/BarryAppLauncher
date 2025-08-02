#include "terminalutil.h"
#include "managers/errormanager.h"
#include "managers/settingsmanager.h"

#include <QStandardPaths>
#include <QProcessEnvironment>
#include <QSet>
#include <QStringList>
#include <QString>

TerminalUtil::TerminalUtil() {}

const QString TerminalUtil::detectTerminal()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString desktopEnv = env.value("XDG_CURRENT_DESKTOP").toLower();

    // DE-specific preferences
    QStringList prioritized;
    if (desktopEnv.contains("gnome")) {
        prioritized << "gnome-terminal" << "tilix";
    } else if (desktopEnv.contains("kde")) {
        prioritized << "konsole" << "yakuake";
    } else if (desktopEnv.contains("xfce")) {
        prioritized << "xfce4-terminal";
    } else if (desktopEnv.contains("lxqt") || desktopEnv.contains("lxde")) {
        prioritized << "lxterminal" << "qterminal";
    } else if (desktopEnv.contains("mate")) {
        prioritized << "mate-terminal";
    } else if (desktopEnv.contains("deepin")) {
        prioritized << "deepin-terminal";
    }

    // Full list of known terminals
    QStringList allKnown = {
        "alacritty", "kitty", "foot", "wezterm",
        "gnome-terminal", "konsole", "xfce4-terminal", "lxterminal",
        "tilix", "terminator", "mate-terminal", "xterm", "urxvt",
        "eterm", "st", "sakura", "deepin-terminal", "qterminal", "yakuake"
    };

    QStringList combined = prioritized;
    combined.append(allKnown);

    QSet<QString> seen;
    QStringList merged;
    for (const QString &term : combined) {
        if (!seen.contains(term)) {
            seen.insert(term);
            merged.append(term);
        }
    }

    // Find the first executable
    for (const QString &term : merged) {
        QString path = QStandardPaths::findExecutable(term);
        if (!path.isEmpty()) {
            return path;
        }
    }

    return QString();
}

const bool TerminalUtil::terminalExists(const QString& path)
{
    QString execPath = QStandardPaths::findExecutable(path);
    return !execPath.isEmpty();
}

const QString TerminalUtil::detectUserShell()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString shell = env.value("SHELL");
    return shell.isEmpty() ? "/bin/bash" : shell;
}

const bool TerminalUtil::launchInTerminal(const QString &path)
{
    QString terminal = SettingsManager::instance()->terminal();
    if (terminal.isEmpty()) {
        ErrorManager::instance()->reportError("No terminal emulator found.");
        return false;
    }

    QString shell = detectUserShell();
    QString shellCmd = QString("'%1'; exec %2").arg(path, shell);
    QStringList args;

    if (terminal.contains("gnome-terminal") ||
        terminal.contains("tilix") ||
        terminal.contains("xfce4-terminal") ||
        terminal.contains("mate-terminal") ||
        terminal.contains("deepin-terminal")) {
        args << "--" << shell << "-c" << shellCmd;
    } else if (terminal.contains("konsole") || terminal.contains("yakuake") ||
               terminal.contains("xterm") || terminal.contains("lxterminal") ||
               terminal.contains("qterminal") || terminal.contains("urxvt") ||
               terminal.contains("alacritty") || terminal.contains("kitty") ||
               terminal.contains("foot") || terminal.contains("wezterm")) {
        args << "-e" << shell << "-c" << shellCmd;
    } else {
        ErrorManager::instance()->reportError("Unsupported terminal: " + terminal);
        return false;
    }

    return QProcess::startDetached(terminal, args);
}

