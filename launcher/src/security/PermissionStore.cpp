#include "PermissionStore.h"

#include "napp/core/json.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

namespace napp::security {

PermissionStore::PermissionStore(Paths paths, QObject* parent)
    : QObject(parent)
    , m_paths(std::move(paths))
{
}

Result<void> PermissionStore::load()
{
    const QString path = QDir(m_paths.configRoot()).filePath(QStringLiteral("permissions.json"));
    auto document = json::readObjectOrEmpty(path);
    if (!document)
        return document.error();

    m_grants.clear();
    const QJsonObject apps = document.value().value(QStringLiteral("apps")).toObject();
    for (auto it = apps.constBegin(); it != apps.constEnd(); ++it) {
        QStringList permissions;
        for (const QJsonValue& value : it.value().toObject().value(QStringLiteral("granted")).toArray())
            permissions.append(value.toString());
        m_grants.insert(it.key(), permissions);
    }
    return Result<void>::ok();
}

QStringList PermissionStore::granted(const QString& appId) const
{
    return m_grants.value(appId);
}

bool PermissionStore::isGranted(const QString& appId, const QString& permission) const
{
    return m_grants.value(appId).contains(permission);
}

Result<void> PermissionStore::setGranted(const QString& appId, const QStringList& permissions)
{
    m_grants.insert(appId, permissions);
    auto saved = save();
    if (saved)
        emit changed(appId);
    return saved;
}

Result<void> PermissionStore::clear(const QString& appId)
{
    if (m_grants.remove(appId) == 0)
        return Result<void>::ok();
    auto saved = save();
    if (saved)
        emit changed(appId);
    return saved;
}

Result<void> PermissionStore::save() const
{
    QJsonObject apps;
    for (auto it = m_grants.constBegin(); it != m_grants.constEnd(); ++it) {
        QJsonArray permissions;
        for (const QString& permission : it.value())
            permissions.append(permission);
        QJsonObject object;
        object.insert(QStringLiteral("granted"), permissions);
        apps.insert(it.key(), object);
    }

    QJsonObject document;
    document.insert(QStringLiteral("schema_version"), 1);
    document.insert(QStringLiteral("apps"), apps);
    return json::writeObjectAtomically(
        QDir(m_paths.configRoot()).filePath(QStringLiteral("permissions.json")), document);
}

} // namespace napp::security
