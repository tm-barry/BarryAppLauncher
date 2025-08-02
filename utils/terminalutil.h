#ifndef TERMINALUTIL_H
#define TERMINALUTIL_H

#include <QString>

class TerminalUtil
{
public:
    TerminalUtil();

    static const QString detectTerminal();
    static const bool terminalExists(const QString& path);
    static const QString detectUserShell();
    static const bool launchInTerminal(const QString &path);
};

#endif // TERMINALUTIL_H
