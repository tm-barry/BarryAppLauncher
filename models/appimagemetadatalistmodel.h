#ifndef APPIMAGEMETADATALISTMODEL_H
#define APPIMAGEMETADATALISTMODEL_H

#pragma once

#include <QAbstractListModel>
#include "appimagemetadata.h"

class AppImageMetadataListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        VersionRole,
        CommentRole,
        TypeRole,
        IconRole,
        ChecksumRole,
        CategoriesRole,
        PathRole,
        IntegrationRole,
        DesktopFilePathRole
    };

    explicit AppImageMetadataListModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    void clear();
    Q_INVOKABLE void addMetadata(AppImageMetadata* metadata);

private:
    QList<AppImageMetadata*> m_items;

signals:
    void countChanged();
};

#endif // APPIMAGEMETADATALISTMODEL_H
