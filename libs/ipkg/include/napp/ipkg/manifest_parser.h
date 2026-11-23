#pragma once

#include "napp/core/result.h"
#include "napp/ipkg/manifest.h"
#include "napp/ipkg/zip_reader.h"

#include <QByteArray>
#include <QString>

namespace napp::ipkg {

// Parses and validates manifest.json. Validation mirrors the published JSON
// Schema plus the semantic rules of IPKG_SPECIFICATION.md §5 and returns the
// spec error codes (E1003–E1005, E1020).
class ManifestParser
{
public:
    static constexpr qint64 kMaxManifestBytes = 1024 * 1024;

    static Result<Manifest> parse(const QByteArray& json, const QString& origin = QStringLiteral("manifest.json"));
    static Result<Manifest> readFrom(const ZipReader& reader);
};

} // namespace napp::ipkg
