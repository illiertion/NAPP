#pragma once

#include "napp/core/paths.h"
#include "napp/core/result.h"
#include "napp/ipkg/limits.h"
#include "napp/ipkg/package_verifier.h"

#include <QString>
#include <QStringList>

namespace napp::ipkg {

struct InstallRequest
{
    QString packagePath;
    QString expectedSha256;  // empty = skip hash check (local file install)
    QString repositoryId;    // recorded in the package marker for origin binding
    bool allowPlatformCheck = true;
};

struct InstalledPackage
{
    QString appId;
    QString name;
    QString version;
    QString directory;
    QString repositoryId;
    QStringList permissions;
    QString entryRelPath;
    QString iconRelPath;

    // Optional window preferences from the manifest's "window" block.
    // 매니페스트의 "window" 블록에서 읽은 창 설정 (없으면 기본값).
    QString windowTitle;
    int windowWidth = 0;
    int windowHeight = 0;
    bool windowTray = false;
};

// Atomic installation into the layout of IPKG_SPECIFICATION.md §7:
// extract into staging, then rename into apps/<id>/<version>, then move the
// `current` pointer. A failure never disturbs an existing installation.
class Installer
{
public:
    explicit Installer(Paths paths, Limits limits = {});

    const Paths& paths() const { return m_paths; }

    Result<InstalledPackage> install(const InstallRequest& request);

    Result<void> remove(const QString& appId, bool purgeData);

    Result<QString> activeVersion(const QString& appId) const;
    Result<InstalledPackage> activePackage(const QString& appId) const;

    // Switches the `current` pointer (used by downgrade / rollback UI).
    Result<void> activate(const QString& appId, const QString& version);

    Result<QStringList> installedAppIds() const;
    Result<QStringList> installedVersions(const QString& appId) const;

    // Removes version directories other than `keepVersion` (keeps data intact).
    Result<void> pruneVersions(const QString& appId, const QString& keepVersion);

private:
    Result<InstalledPackage> describe(const QString& appId, const QString& version) const;
    Result<void> extractAll(const QString& packagePath, const QString& destination) const;

    Paths m_paths;
    Limits m_limits;
    PackageVerifier m_verifier;
};

} // namespace napp::ipkg
