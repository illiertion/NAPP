#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace napp::ui {

// Generic list model that exposes QVariantMap rows to QML with stable role
// names. Keeps the UI layer free of per-screen model boilerplate.
class MapListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit MapListModel(QStringList roles, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reset(QVector<QVariantMap> rows);
    int count() const { return int(m_rows.size()); }
    Q_INVOKABLE QVariantMap get(int row) const;

private:
    QStringList m_roleOrder;
    QHash<int, QByteArray> m_roles;
    QVector<QVariantMap> m_rows;
};

} // namespace napp::ui
