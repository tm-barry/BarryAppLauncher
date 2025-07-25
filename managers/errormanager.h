#ifndef ERRORMANAGER_H
#define ERRORMANAGER_H

#include <QObject>

class ErrorManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

public:
    static ErrorManager* instance();

    QString lastError() const;
    Q_INVOKABLE void reportError(const QString& error);

private:
    explicit ErrorManager(QObject *parent = nullptr);
    QString m_lastError;

    Q_DISABLE_COPY(ErrorManager)

signals:
    void errorOccurred(const QString& message);
};

#endif // ERRORMANAGER_H
