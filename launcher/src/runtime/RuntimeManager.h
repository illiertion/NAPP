#pragma once

#include "napp/core/paths.h"
#include "napp/core/result.h"

#include <QList>
#include <QObject>
#include <QString>

namespace napp::core {

struct InstalledRuntime
{
    QString version;
    QString chromiumVersion;
    QString platform;
    QString directory;
    QString hostExecutable;
    bool valid = false;
    QString problem;
};

// Manages NAPP Web Runtime installations (spec §6): version discovery,
// activation, compatibility checks and registration of a prepared runtime
// directory. The runtime is installed separately from the launcher.
//
// A runtime directory contains runtime.json:
//   { "format": "NAPP-RUNTIME", "version": "1.0.0",
//     "chromium_version": "158.0.8127.18", "platform": "windows-x64",
//     "host": "napp-runtime-host.exe" }
class RuntimeManager : public QObject
{
    Q_OBJECT

public:
    explicit RuntimeManager(Paths paths, QObject* parent = nullptr);

    void scan();

    const QList<InstalledRuntime>& installed() const { return m_installed; }
    const InstalledRuntime* find(const QString& version) const;

    QString activeVersion() const;
    const InstalledRuntime* active() const;

    Result<void> activate(const QString& version);
    Result<void> uninstall(const QString& version);

    // Copies a prepared runtime directory (e.g. an installed CEF bundle) into
    // the runtime store and validates runtime.json.
    Result<InstalledRuntime> registerFromDirectory(const QString& sourceDirectory);

    // True when an active runtime satisfies the manifest's minimum version.
    bool supports(const QString& minimumVersion) const;

    static QString hostExecutableName();
    static QString runtimeManifestName();

signals:
    void changed();

private:
    Result<void> writeActiveVersion(const QString& version) const;

    Paths m_paths;
    QList<InstalledRuntime> m_installed;
};

} // namespace napp::core

Q_DECLARE_METATYPE(napp::core::InstalledRuntime)
