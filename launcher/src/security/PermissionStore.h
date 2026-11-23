#pragma once

#include "napp/core/paths.h"
#include "napp/core/result.h"

#include <QHash>
#include <QObject>
#include <QStringList>

namespace napp::security {

// Records which manifest permissions the user approved for each app.
// The runtime host receives the approved list at spawn time; the app page can
// never escalate it by itself (spec §12.3).
class PermissionStore : public QObject
{
    Q_OBJECT

public:
    explicit PermissionStore(Paths paths, QObject* parent = nullptr);

    Result<void> load();

    QStringList granted(const QString& appId) const;
    bool isGranted(const QString& appId, const QString& permission) const;

    // Stores the intersection of the manifest request and the user's choices.
    Result<void> setGranted(const QString& appId, const QStringList& permissions);
    Result<void> clear(const QString& appId);

signals:
    void changed(const QString& appId);

private:
    Result<void> save() const;

    Paths m_paths;
    QHash<QString, QStringList> m_grants;
};

} // namespace napp::security
