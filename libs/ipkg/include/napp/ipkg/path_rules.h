#pragma once

#include "napp/core/result.h"

#include <QString>

namespace napp::ipkg {

// Entry name rules from IPKG_SPECIFICATION.md §3.3. The same function is used
// by the launcher, IPKG Studio and the runtime host so all three agree.
namespace paths {

// Validates a ZIP entry name. `isDirectory` relaxes only the trailing-slash rule.
Result<void> validateEntryName(const QString& name, bool isDirectory, int maxPathBytes = 512);

// True for names Windows refuses to create (CON, NUL, COM1, ...).
bool isReservedWindowsName(const QString& segment);

// Case-folded key used for duplicate detection so packages behave the same on
// case-sensitive Linux and case-insensitive Windows filesystems.
QString duplicationKey(const QString& name);

} // namespace paths
} // namespace napp::ipkg
