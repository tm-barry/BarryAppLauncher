#ifndef UPDATERFILTERMODEL_H
#define UPDATERFILTERMODEL_H

#include "utils/updater/updaterfactory.h"

#include <QObject>

class UpdaterFilterModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString field READ field WRITE setField NOTIFY fieldChanged)
    Q_PROPERTY(QString pattern READ pattern WRITE setPattern NOTIFY patternChanged)

public:
    explicit UpdaterFilterModel(QObject* parent = nullptr)
        : QObject(parent) {}

    static UpdaterFilterModel* createFromUtil(const UpdaterFilter& filter, QObject* parent = nullptr)
    {
        auto* filterModel = new UpdaterFilterModel(parent);
        filterModel->setField(filter.field);
        filterModel->setPattern(filter.pattern);

        return filterModel;
    }

    QString field() const { return m_field; }
    void setField(const QString& value) {
        if (m_field != value) {
            m_field = value;
            emit fieldChanged();
        }
    }

    QString pattern() const { return m_pattern; }
    void setPattern(const QString& value) {
        if (m_pattern != value) {
            m_pattern = value;
            emit patternChanged();
        }
    }

signals:
    void fieldChanged();
    void patternChanged();

private:
    QString m_field;
    QString m_pattern;
};


#endif // UPDATERFILTERMODEL_H
