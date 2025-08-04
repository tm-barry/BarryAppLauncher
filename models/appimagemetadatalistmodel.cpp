#include "appimagemetadatalistmodel.h"

AppImageMetadataListModel::AppImageMetadataListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int AppImageMetadataListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.count();
}

QVariant AppImageMetadataListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.count())
        return QVariant();

    AppImageMetadata* item = m_items.at(index.row());

    switch (role) {
    case NameRole: return item->name();
    case VersionRole: return item->version();
    case CommentRole: return item->comment();
    case TypeRole: return item->type();
    case IconRole: return item->icon();
    case ChecksumRole: return item->checksum();
    case CategoriesRole: return item->categories();
    case PathRole: return item->path();
    case IntegrationRole: return static_cast<int>(item->integration());
    case DesktopFilePathRole: return item->desktopFilePath();
    case ExecutableRole: return item->executable();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> AppImageMetadataListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[VersionRole] = "version";
    roles[CommentRole] = "comment";
    roles[TypeRole] = "type";
    roles[IconRole] = "icon";
    roles[ChecksumRole] = "checksum";
    roles[CategoriesRole] = "categories";
    roles[PathRole] = "path";
    roles[IntegrationRole] = "integration";
    roles[DesktopFilePathRole] = "desktopFilePath";
    roles[ExecutableRole] = "executable";
    return roles;
}

void AppImageMetadataListModel::addMetadata(AppImageMetadata* metadata)
{
    beginInsertRows(QModelIndex(), m_items.count(), m_items.count());
    m_items.append(metadata);
    endInsertRows();
    emit countChanged();
}

void AppImageMetadataListModel::clear()
{
    beginResetModel();
    qDeleteAll(m_items);
    m_items.clear();
    endResetModel();
    emit countChanged();
}
