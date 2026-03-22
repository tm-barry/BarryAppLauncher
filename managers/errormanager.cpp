#include "errormanager.h"

#include <QDebug>

// ----------------- Public -----------------

ErrorManager* ErrorManager::instance() {
    static ErrorManager singleton;
    return &singleton;
}

void ErrorManager::reportError(const QString& error) {
    if (m_mode == Mode::Gui) {
        qCritical() << error;
    }
    emit messageOccurred(error, Error);
}

void ErrorManager::reportWarning(const QString& warning) {
    if (m_mode == Mode::Gui) {
        qWarning() << warning;
    }
    emit messageOccurred(warning, Warning);
}

// ----------------- Private -----------------

ErrorManager::ErrorManager(QObject* parent)
    : QObject(parent) {}
