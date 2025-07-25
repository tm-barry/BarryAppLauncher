#include "errormanager.h"

#include <QDebug>

// ----------------- Public -----------------

ErrorManager* ErrorManager::instance() {
    static ErrorManager singleton;
    return &singleton;
}

QString ErrorManager::lastError() const {
    return m_lastError;
}

void ErrorManager::reportError(const QString& error) {
    m_lastError = error;
    qWarning() << "Reported Error:" << error;
    emit errorOccurred(error);
}

// ----------------- Private -----------------

ErrorManager::ErrorManager(QObject* parent)
    : QObject(parent) {}
