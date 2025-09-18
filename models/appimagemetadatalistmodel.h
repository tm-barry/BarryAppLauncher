#ifndef APPIMAGEMETADATALISTMODEL_H
#define APPIMAGEMETADATALISTMODEL_H

#pragma once

#include <QAbstractListModel>
#include "appimagemetadata.h"

class AppImageMetadataListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool hasAnyNewRelease READ hasAnyNewRelease NOTIFY hasAnyNewReleaseChanged)
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
        DesktopFilePathRole,
        ExecutableRole,
        HasNewReleaseRole,
        UpdaterReleasesRole
    };

    explicit AppImageMetadataListModel(QObject* parent = nullptr);

    const QList<AppImageMetadata*>& items() const;
    bool hasAnyNewRelease() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    void clear();
    void updateItem(int row);
    void updateAllItems();
    Q_INVOKABLE void addMetadata(AppImageMetadata* metadata);

private:
    QList<AppImageMetadata*> m_items;

signals:
    void countChanged();
    void hasAnyNewReleaseChanged();
};

#endif // APPIMAGEMETADATALISTMODEL_H
