#pragma once

#include <QString>
#include <QStringList>

namespace napp {

// Platform identifiers per IPKG_SPECIFICATION.md §5.1: "{os}-{arch}".
QString currentPlatformId();

// False when the identifier is unknown or reserved for a future launcher.
bool isSupportedPlatform(const QString& platformId);

// Human readable description used in UI messages.
QString platformDisplayName(const QString& platformId);

QStringList allPlatformIds();

} // namespace napp
