#ifndef UPDATERRELEASEMODEL_H
#define UPDATERRELEASEMODEL_H

#include <QObject>

class UpdaterReleaseModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString date READ date WRITE setDate NOTIFY dateChanged)
    Q_PROPERTY(QString download READ download WRITE setDownload NOTIFY downloadChanged)
    Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY versionChanged)
    Q_PROPERTY(bool isNew READ isNew WRITE setIsNew NOTIFY isNewChanged)
    Q_PROPERTY(bool isSelected READ isSelected WRITE setIsSelected NOTIFY isSelectedChanged)
public:
    explicit UpdaterReleaseModel(QObject *parent = nullptr)
        : QObject(parent) {}

    QString date() const { return m_date; }
    void setDate(const QString& value) {
        if (m_date != value) {
            m_date = value;
            emit dateChanged();
        }
    }

    QString download() const { return m_download; }
    void setDownload(const QString& value) {
        if (m_download != value) {
            m_download = value;
            emit downloadChanged();
        }
    }

    QString version() const { return m_version; }
    void setVersion(const QString& value) {
        if (m_version != value) {
            m_version = value;
            emit versionChanged();
        }
    }

    bool isNew() const { return m_isNew; }
    void setIsNew(bool value) {
        if (m_isNew != value) {
            m_isNew = value;
            emit isNewChanged();
        }
    }

    bool isSelected() const { return m_isSelected; }
    void setIsSelected(bool value) {
        if (m_isSelected != value) {
            m_isSelected = value;
            emit isSelectedChanged();
        }
    }

signals:
    void dateChanged();
    void downloadChanged();
    void versionChanged();
    void isNewChanged();
    void isSelectedChanged();

private:
    QString m_date = QString();
    QString m_download = QString();
    QString m_version = QString();
    bool m_isNew = false;
    bool m_isSelected = false;
};

#endif // UPDATERRELEASEMODEL_H
