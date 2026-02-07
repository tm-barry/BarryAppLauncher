#include "errormanager.h"

#include <QDebug>

// ----------------- Public -----------------

ErrorManager* ErrorManager::instance() {
    static ErrorManager singleton;
    return &singleton;
}

void ErrorManager::reportError(const QString& error) {
    qCritical() << error;
    emit messageOccurred(error, Error);
}

void ErrorManager::reportWarning(const QString& warning) {
    qWarning() << warning;
    emit messageOccurred(warning, Warning);
}

// ----------------- Private -----------------

ErrorManager::ErrorManager(QObject* parent)
    : QObject(parent) {}
