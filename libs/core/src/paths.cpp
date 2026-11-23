#include "napp/core/paths.h"

#include <QDir>
#include <QStandardPaths>

namespace napp {
namespace {

QString joinPath(const QString& base, const QString& first)
{
    return QDir(base).filePath(first);
}

} // namespace

Paths::Paths(QString configRoot, QString dataRoot)
    : m_configRoot(std::move(configRoot))
    , m_dataRoot(std::move(dataRoot))
{
}

Paths Paths::standard()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/illiertion/napp/config");
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/illiertion/napp");
    return Paths(config, data);
}

Paths Paths::forRoot(const QString& root)
{
    return Paths(QDir(root).filePath(QStringLiteral("config")), root);
}

QString Paths::appsDir() const { return joinPath(m_dataRoot, QStringLiteral("apps")); }

QString Paths::appVersionDir(const QString& appId, const QString& version) const
{
    return QDir(appsDir()).filePath(appId + QLatin1Char('/') + version);
}

QString Paths::appPointerFile(const QString& appId) const
{
    return QDir(appsDir()).filePath(appId + QStringLiteral("/current"));
}

QString Paths::dataDir(const QString& appId) const
{
    return QDir(joinPath(m_dataRoot, QStringLiteral("data"))).filePath(appId);
}

QString Paths::cacheDir() const { return joinPath(m_dataRoot, QStringLiteral("cache")); }

QString Paths::stagingDir() const { return joinPath(m_dataRoot, QStringLiteral("staging")); }

QString Paths::runtimeDir() const { return joinPath(m_dataRoot, QStringLiteral("runtime")); }

QString Paths::runtimeVersionDir(const QString& version) const
{
    return QDir(runtimeDir()).filePath(version);
}

QString Paths::registryFile() const { return joinPath(m_configRoot, QStringLiteral("registry.json")); }

QString Paths::repositoriesFile() const
{
    return joinPath(m_configRoot, QStringLiteral("repositories.json"));
}

QString Paths::settingsFile() const { return joinPath(m_configRoot, QStringLiteral("settings.json")); }

QString Paths::logFile() const { return joinPath(m_configRoot, QStringLiteral("napp.log")); }

bool Paths::ensureLayout() const
{
    const QStringList directories{
        m_configRoot,
        appsDir(),
        joinPath(m_dataRoot, QStringLiteral("data")),
        cacheDir(),
        stagingDir(),
        runtimeDir(),
    };
    for (const QString& directory : directories) {
        if (!QDir().mkpath(directory))
            return false;
    }
    return true;
}

bool Paths::isInside(const QString& base, const QString& candidate)
{
    if (base.isEmpty() || candidate.isEmpty())
        return false;

    const QString normalizedBase = QDir::cleanPath(QDir(base).absolutePath());
    const QString normalizedCandidate = QDir::cleanPath(QDir(candidate).absolutePath());

#if defined(Q_OS_WIN)
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif

    if (normalizedCandidate.compare(normalizedBase, sensitivity) == 0)
        return true;

    const QString prefix = normalizedBase.endsWith(QLatin1Char('/')) ? normalizedBase
                                                                    : normalizedBase + QLatin1Char('/');
    return normalizedCandidate.startsWith(prefix, sensitivity);
}

QString Paths::resolveInside(const QString& base, const QString& relative)
{
    if (base.isEmpty() || relative.isEmpty())
        return {};
    if (relative.startsWith(QLatin1Char('/')) || relative.startsWith(QLatin1Char('\\')))
        return {};
    if (relative.contains(QLatin1Char('\\')))
        return {};

    const QString joined = QDir(base).filePath(relative);
    if (!isInside(base, joined))
        return {};
    return QDir::cleanPath(joined);
}

} // namespace napp
