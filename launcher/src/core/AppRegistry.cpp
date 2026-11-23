#include "AppRegistry.h"

#include "napp/core/json.h"
#include "napp/core/logging.h"

#include <QJsonArray>
#include <QJsonObject>

namespace napp::core {

AppRegistry::AppRegistry(Paths paths, QObject* parent)
    : QObject(parent)
    , m_paths(std::move(paths))
{
}

Result<void> AppRegistry::load()
{
    auto document = json::readObjectOrEmpty(m_paths.registryFile());
    if (!document)
        return document.error();

    m_entries.clear();
    const QJsonObject apps = document.value().value(QStringLiteral("apps")).toObject();
    for (auto it = apps.constBegin(); it != apps.constEnd(); ++it) {
        const QJsonObject object = it.value().toObject();
        RegistryEntry entry;
        entry.appId = it.key();
        entry.name = object.value(QStringLiteral("name")).toString();
        entry.version = object.value(QStringLiteral("version")).toString();
        entry.repositoryId = object.value(QStringLiteral("repository_id")).toString();
        entry.installedAt = QDateTime::fromString(object.value(QStringLiteral("installed_at")).toString(),
                                                  Qt::ISODate);
        for (const QJsonValue& permission : object.value(QStringLiteral("permissions")).toArray())
            entry.permissions.append(permission.toString());
        if (entry.isValid())
            m_entries.append(entry);
    }
    return Result<void>::ok();
}

const RegistryEntry* AppRegistry::find(const QString& appId) const
{
    for (const RegistryEntry& entry : m_entries) {
        if (entry.appId == appId)
            return &entry;
    }
    return nullptr;
}

Result<void> AppRegistry::upsert(const RegistryEntry& entry)
{
    bool replaced = false;
    for (RegistryEntry& existing : m_entries) {
        if (existing.appId == entry.appId) {
            existing = entry;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        m_entries.append(entry);

    auto saved = save();
    if (saved)
        emit changed();
    return saved;
}

Result<void> AppRegistry::remove(const QString& appId)
{
    for (int index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).appId == appId) {
            m_entries.removeAt(index);
            auto saved = save();
            if (saved)
                emit changed();
            return saved;
        }
    }
    return Result<void>::ok();
}

Result<void> AppRegistry::reconcile(const QList<RegistryEntry>& discovered)
{
    bool modified = false;
    for (const RegistryEntry& entry : discovered) {
        RegistryEntry* existing = nullptr;
        for (RegistryEntry& candidate : m_entries)
            if (candidate.appId == entry.appId)
                existing = &candidate;
        if (!existing) {
            m_entries.append(entry);
            modified = true;
            qInfo("registry recovered entry for %s", qPrintable(entry.appId));
            continue;
        }
        // The active package can move on without the registry knowing: installing
        // from a file (--install-package) or updating a version never rewrote it,
        // so the library kept showing the old version. Refresh in place instead.
        // 파일 설치/업데이트는 레지스트리를 거치지 않으므로, 실제 활성 패키지의
        // 버전·이름·권한으로 기존 항목을 갱신한다.
        if (existing->version == entry.version && existing->name == entry.name
            && existing->permissions == entry.permissions)
            continue;
        existing->version = entry.version;
        existing->name = entry.name;
        existing->permissions = entry.permissions;
        if (existing->repositoryId.isEmpty())
            existing->repositoryId = entry.repositoryId;
        modified = true;
        qInfo("registry updated %s to v%s", qPrintable(entry.appId), qPrintable(entry.version));
    }
    if (!modified)
        return Result<void>::ok();

    auto saved = save();
    if (saved)
        emit changed();
    return saved;
}

Result<void> AppRegistry::save() const
{
    QJsonObject apps;
    for (const RegistryEntry& entry : m_entries) {
        QJsonObject object;
        object.insert(QStringLiteral("name"), entry.name);
        object.insert(QStringLiteral("version"), entry.version);
        object.insert(QStringLiteral("repository_id"), entry.repositoryId);
        object.insert(QStringLiteral("installed_at"), entry.installedAt.toString(Qt::ISODate));
        QJsonArray permissions;
        for (const QString& permission : entry.permissions)
            permissions.append(permission);
        object.insert(QStringLiteral("permissions"), permissions);
        apps.insert(entry.appId, object);
    }

    QJsonObject document;
    document.insert(QStringLiteral("schema_version"), 1);
    document.insert(QStringLiteral("apps"), apps);
    return json::writeObjectAtomically(m_paths.registryFile(), document);
}

} // namespace napp::core
