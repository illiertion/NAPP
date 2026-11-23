#pragma once

#include "napp/core/paths.h"
#include "napp/core/result.h"
#include "napp/ipkg/installer.h"

#include <QHash>
#include <QObject>
#include <QStringList>

class QProcess;

namespace napp::core {

class RuntimeManager;

// Starts one runtime host process per app (architecture §3.1 model B): the CEF
// binaries are shared on disk, while each app gets its own process, profile
// directory and permission set.
class AppLauncher : public QObject
{
    Q_OBJECT

public:
    AppLauncher(Paths paths, RuntimeManager* runtime, QObject* parent = nullptr);

    // Session values the runtime hands to the page as window.napp (language,
    // theme, accent and the store base URL for on-demand docs).
    struct Environment
    {
        QString language = QStringLiteral("en");
        bool dark = true;
        QString accent = QStringLiteral("blue");
        QString repositoryUrl;
    };

    void setEnvironment(const Environment& environment);

    Result<void> launch(const ipkg::InstalledPackage& package, const QStringList& permissions);
    bool isRunning(const QString& appId) const;
    void terminate(const QString& appId);

signals:
    void launchFailed(const QString& appId, const QString& message);
    void processExited(const QString& appId, int exitCode);

private:
    void onProcessFinished(const QString& appId);

    Paths m_paths;
    RuntimeManager* m_runtime = nullptr;
    Environment m_environment;
    QHash<QString, QProcess*> m_processes;
};

} // namespace napp::core
