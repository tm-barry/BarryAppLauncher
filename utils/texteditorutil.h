#ifndef TEXTEDITORUTIL_H
#define TEXTEDITORUTIL_H

#include <QString>

class TextEditorUtil
{
public:
    TextEditorUtil();

    static const QString detectTextEditor();
    static const bool textEditorExists(const QString& path);
    static const bool launchInTextEditor(const QString &path);

private:
    static const QString detectTextEditorByEnv();
    static const QString detectTextEditorByMime();
    static const QString detectTextEditorByPath();
    static const QString getExecFromDesktopFile(const QString& path);
};

#endif // TEXTEDITORUTIL_H
