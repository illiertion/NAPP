#include "napp/core/platform.h"

#include <QSysInfo>

namespace napp {
namespace {

QString architectureId()
{
    const QString arch = QSysInfo::currentCpuArchitecture().toLower();
    if (arch == QStringLiteral("x86_64"))
        return QStringLiteral("x64");
    if (arch == QStringLiteral("arm64") || arch == QStringLiteral("aarch64"))
        return QStringLiteral("arm64");
    if (arch == QStringLiteral("x86") || arch == QStringLiteral("i386"))
        return QStringLiteral("x86");
    return arch;
}

} // namespace

QString currentPlatformId()
{
    const QString arch = architectureId();
#if defined(Q_OS_WIN)
    return QStringLiteral("windows-") + arch;
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux-") + arch;
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos-") + arch;
#else
    return QStringLiteral("unknown-") + arch;
#endif
}

QStringList allPlatformIds()
{
    return {
        QStringLiteral("windows-x64"),
        QStringLiteral("windows-arm64"),
        QStringLiteral("linux-x64"),
        QStringLiteral("linux-arm64"),
        QStringLiteral("macos-x64"),
        QStringLiteral("macos-arm64"),
    };
}

bool isSupportedPlatform(const QString& platformId)
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return platformId == currentPlatformId();
#else
    Q_UNUSED(platformId);
    return false;
#endif
}

QString platformDisplayName(const QString& platformId)
{
    if (platformId == QStringLiteral("windows-x64"))
        return QStringLiteral("Windows (x64)");
    if (platformId == QStringLiteral("windows-arm64"))
        return QStringLiteral("Windows (ARM64)");
    if (platformId == QStringLiteral("linux-x64"))
        return QStringLiteral("Linux (x64)");
    if (platformId == QStringLiteral("linux-arm64"))
        return QStringLiteral("Linux (ARM64)");
    if (platformId == QStringLiteral("macos-x64"))
        return QStringLiteral("macOS (Intel)");
    if (platformId == QStringLiteral("macos-arm64"))
        return QStringLiteral("macOS (Apple Silicon)");
    return platformId;
}

} // namespace napp
