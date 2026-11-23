#include "MapListModel.h"

namespace napp::ui {

MapListModel::MapListModel(QStringList roles, QObject* parent)
    : QAbstractListModel(parent)
    , m_roleOrder(std::move(roles))
{
    for (int index = 0; index < m_roleOrder.size(); ++index)
        m_roles.insert(Qt::UserRole + index, m_roleOrder.at(index).toUtf8());
}

int MapListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return int(m_rows.size());
}

QVariant MapListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const int roleIndex = role - Qt::UserRole;
    if (roleIndex < 0 || roleIndex >= m_roleOrder.size())
        return {};
    return m_rows.at(index.row()).value(m_roleOrder.at(roleIndex));
}

QHash<int, QByteArray> MapListModel::roleNames() const
{
    return m_roles;
}

void MapListModel::reset(QVector<QVariantMap> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

QVariantMap MapListModel::get(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    return m_rows.at(row);
}

} // namespace napp::ui
