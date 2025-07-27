#include "clipboardmanager.h"

#include <QGuiApplication>
#include <QClipboard>

// ----------------- Public -----------------

ClipboardManager* ClipboardManager::instance() {
    static ClipboardManager singleton;
    return &singleton;
}

void ClipboardManager::copyToClipboard(const QString& text) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard)
        clipboard->setText(text);
}

// ----------------- Private -----------------

ClipboardManager::ClipboardManager(QObject *parent)
    : QObject{parent}
{}
