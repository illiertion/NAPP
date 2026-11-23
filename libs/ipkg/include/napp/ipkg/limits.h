#pragma once

#include <QtGlobal>

namespace napp::ipkg {

// Hard caps from IPKG_SPECIFICATION.md §3.2. They are enforced against the
// declared sizes at open time and against the streamed byte count while
// extracting, so a forged central directory cannot bypass them.
struct Limits
{
    int maxEntries = 20000;
    quint64 maxEntrySize = 128ull * 1024 * 1024;
    quint64 maxTotalSize = 512ull * 1024 * 1024;
    int maxSingleEntryRatio = 200;
    int maxTotalRatio = 100;
    int maxPathBytes = 512;
    quint64 maxManifestBytes = 1ull * 1024 * 1024;
    quint64 maxPackageBytes = 512ull * 1024 * 1024;
};

} // namespace napp::ipkg
