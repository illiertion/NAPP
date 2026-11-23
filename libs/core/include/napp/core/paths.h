#pragma once

#include <QString>

namespace napp {

// Resolves every on-disk location used by the launcher and runtime host.
// Layout follows IPKG_SPECIFICATION.md §7; tests can point it at a temp root.
class Paths
{
public:
    Paths() = default;
    Paths(QString configRoot, QString dataRoot);

    // %APPDATA%\ILLIERTION\NAPP\config + %LOCALAPPDATA%\ILLIERTION\NAPP
    // (Linux: XDG config/data). No directories are created.
    static Paths standard();

    // Single root, used by tests and the runtime host sandbox.
    static Paths forRoot(const QString& root);

    bool isValid() const { return !m_configRoot.isEmpty() && !m_dataRoot.isEmpty(); }
    const QString& configRoot() const { return m_configRoot; }
    const QString& dataRoot() const { return m_dataRoot; }

    QString appsDir() const;
    QString appVersionDir(const QString& appId, const QString& version) const;
    QString appPointerFile(const QString& appId) const;
    QString dataDir(const QString& appId) const;
    QString cacheDir() const;
    QString stagingDir() const;
    QString runtimeDir() const;
    QString runtimeVersionDir(const QString& version) const;
    QString registryFile() const;
    QString repositoriesFile() const;
    QString settingsFile() const;
    QString logFile() const;

    // Creates the base directories (config, apps, data, cache, staging, runtime).
    bool ensureLayout() const;

    // True when `candidate` stays inside `base` after normalisation.
    static bool isInside(const QString& base, const QString& candidate);

    // Joins and normalises `relative` under `base`; returns an empty string
    // when the result would escape `base`.
    static QString resolveInside(const QString& base, const QString& relative);

private:
    QString m_configRoot;
    QString m_dataRoot;
};

} // namespace napp
