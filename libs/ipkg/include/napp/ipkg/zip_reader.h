#pragma once

#include "napp/core/result.h"
#include "napp/ipkg/limits.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <memory>

namespace napp::ipkg {

struct ZipEntry
{
    QString name;
    quint64 uncompressedSize = 0;
    quint64 compressedSize = 0;
    quint16 method = 0;
    quint32 crc32 = 0;
    bool isDirectory = false;
    bool isSymlink = false;
    bool isEncrypted = false;
};

// Read-only ZIP access with IPKG safety rules applied while opening:
// path rules (§3.3), duplicate detection, resource caps (§3.2), encryption and
// link rejection. Callers only ever see entries that passed validation.
class ZipReader
{
public:
    ZipReader();
    ~ZipReader();
    ZipReader(const ZipReader&) = delete;
    ZipReader& operator=(const ZipReader&) = delete;

    Result<void> open(const QString& archivePath, const Limits& limits = {});
    void close();

    bool isOpen() const;
    const QList<ZipEntry>& entries() const;
    const ZipEntry* find(const QString& name) const;
    quint64 totalUncompressedSize() const;

    // Extracts an entry into memory (bounded by maxBytes).
    Result<QByteArray> read(const QString& name, quint64 maxBytes) const;

    // Streams an entry to `destinationFile`, enforcing the per-entry cap.
    Result<void> extractTo(const QString& name, const QString& destinationFile,
                           quint64 maxBytes) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace napp::ipkg
