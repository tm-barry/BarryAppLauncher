#ifndef CLIPBOARDMANAGER_H
#define CLIPBOARDMANAGER_H

#pragma once

#include <QObject>

class ClipboardManager : public QObject
{
    Q_OBJECT
public:
    static ClipboardManager* instance();

    Q_INVOKABLE void copyToClipboard(const QString& text);

private:
    explicit ClipboardManager(QObject *parent = nullptr);

signals:
};

#endif // CLIPBOARDMANAGER_H
