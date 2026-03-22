#ifndef ERRORMANAGER_H
#define ERRORMANAGER_H

#pragma once

#include <QObject>

class ErrorManager : public QObject
{
    Q_OBJECT

public:
    static ErrorManager* instance();

    enum MessageType {
        Error,
        Warning
    };
    Q_ENUM(MessageType);

    Q_INVOKABLE void reportError(const QString& error);
    Q_INVOKABLE void reportWarning(const QString& warning);

    enum class Mode {
        Gui,
        Cli
    };

    void setMode(Mode mode) { m_mode = mode; }

private:
    explicit ErrorManager(QObject *parent = nullptr);

    Mode m_mode = Mode::Gui;

    Q_DISABLE_COPY(ErrorManager)

signals:
    void messageOccurred(const QString& message, const ErrorManager::MessageType messageType);
};

#endif // ERRORMANAGER_H
