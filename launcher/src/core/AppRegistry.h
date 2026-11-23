#pragma once

#include "napp/core/paths.h"
#include "napp/core/result.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QStringList>

namespace napp::core {

struct RegistryEntry
{
    QString appId;
    QString name;
    QString version;
    QString repositoryId;
    QStringList permissions;
    QDateTime installedAt;

    bool isValid() const { return !appId.isEmpty() && !version.isEmpty(); }
};

// `config/registry.json`: the launcher's record of installed apps, their origin
// repository (used for update checks) and granted permissions. It is not part
// of the IPKG spec and can always be rebuilt from the apps/ directory.
class AppRegistry : public QObject
{
    Q_OBJECT

public:
    explicit AppRegistry(Paths paths, QObject* parent = nullptr);

    Result<void> load();

    const QList<RegistryEntry>& entries() const { return m_entries; }
    const RegistryEntry* find(const QString& appId) const;

    Result<void> upsert(const RegistryEntry& entry);
    Result<void> remove(const QString& appId);

    // Rebuilds entries for apps present on disk but missing from the registry.
    Result<void> reconcile(const QList<RegistryEntry>& discovered);

signals:
    void changed();

private:
    Result<void> save() const;

    Paths m_paths;
    QList<RegistryEntry> m_entries;
};

} // namespace napp::core
