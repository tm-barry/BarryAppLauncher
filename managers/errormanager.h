#ifndef ERRORMANAGER_H
#define ERRORMANAGER_H

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

private:
    explicit ErrorManager(QObject *parent = nullptr);

    Q_DISABLE_COPY(ErrorManager)

signals:
    void messageOccurred(const QString& message, const ErrorManager::MessageType messageType);
};

#endif // ERRORMANAGER_H
