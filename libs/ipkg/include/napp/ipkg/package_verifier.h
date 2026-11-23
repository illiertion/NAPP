#pragma once

#include "napp/core/result.h"
#include "napp/ipkg/limits.h"
#include "napp/ipkg/manifest.h"
#include "napp/ipkg/zip_reader.h"

#include <QString>
#include <QStringList>

namespace napp::ipkg {

struct PackageInfo
{
    Manifest manifest;
    QString packagePath;
    qint64 packageSize = 0;
    QString sha256;
    QList<ZipEntry> entries;
    QStringList warnings;
};

// Implements the verification order of IPKG_SPECIFICATION.md §6 steps 1–5 and 7.
// Download-time checks (hash) are performed when an expected digest is given;
// platform compatibility is a separate, explicit decision (checkPlatform).
class PackageVerifier
{
public:
    explicit PackageVerifier(Limits limits = {});

    Result<PackageInfo> verify(const QString& packagePath, const QString& expectedSha256 = QString()) const;

    // E1022 when the current OS/arch is not listed by the manifest.
    Result<void> checkPlatform(const Manifest& manifest) const;

    static QString computeSha256(const QString& path);
    static bool isValidSha256(const QString& value);

private:
    Limits m_limits;
};

} // namespace napp::ipkg
