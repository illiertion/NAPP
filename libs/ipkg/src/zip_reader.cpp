#include "napp/ipkg/zip_reader.h"

#include "napp/ipkg/path_rules.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include "miniz.h"

#include <array>

namespace napp::ipkg {
namespace {

constexpr int kZipSignatureSize = 4;

bool looksLikeZip(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray header = file.read(kZipSignatureSize);
    if (header.size() < kZipSignatureSize)
        return false;
    // "PK\003\004" (local header), "PK\005\006" (empty archive) or "PK\007\010".
    return header == QByteArray("PK\x03\x04", 4) || header == QByteArray("PK\x05\x06", 4)
        || header == QByteArray("PK\x07\x08", 4);
}

struct ExtractContext
{
    QFile* file = nullptr;
    quint64 written = 0;
    quint64 maxBytes = 0;
    bool overflow = false;
};

size_t writeCallback(void* opaque, mz_uint64 fileOffset, const void* data, size_t size)
{
    auto* context = static_cast<ExtractContext*>(opaque);
    if (context->overflow)
        return 0;
    if (fileOffset != context->written)
        return 0; // non-sequential output is never expected for an entry
    if (context->written + size > context->maxBytes) {
        context->overflow = true;
        return 0;
    }
    const qint64 written = context->file->write(static_cast<const char*>(data), static_cast<qint64>(size));
    if (written != static_cast<qint64>(size))
        return 0;
    context->written += size;
    return size;
}

} // namespace

struct ZipReader::Impl
{
    mz_zip_archive archive{};
    bool open = false;
    QList<ZipEntry> entries;
    quint64 totalUncompressed = 0;
    quint64 totalCompressed = 0;
    QHash<QString, int> indexByName;
};

ZipReader::ZipReader() = default;

ZipReader::~ZipReader()
{
    close();
}

bool ZipReader::isOpen() const
{
    return m_impl && m_impl->open;
}

const QList<ZipEntry>& ZipReader::entries() const
{
    static const QList<ZipEntry> empty;
    return m_impl ? m_impl->entries : empty;
}

quint64 ZipReader::totalUncompressedSize() const
{
    return m_impl ? m_impl->totalUncompressed : 0;
}

void ZipReader::close()
{
    if (!m_impl)
        return;
    if (m_impl->open) {
        mz_zip_reader_end(&m_impl->archive);
        m_impl->open = false;
    }
    m_impl.reset();
}

Result<void> ZipReader::open(const QString& archivePath, const Limits& limits)
{
    close();

    if (!QFileInfo::exists(archivePath))
        return Result<void>::fail(ErrorCode::ZipInvalid, QStringLiteral("패키지 파일이 없습니다: %1").arg(archivePath));

    const qint64 fileSize = QFileInfo(archivePath).size();
    if (fileSize <= 0 || static_cast<quint64>(fileSize) > limits.maxPackageBytes)
        return Result<void>::fail(ErrorCode::PackageTooLarge,
                                  QStringLiteral("패키지 크기가 허용 범위를 벗어났습니다: %1 bytes").arg(fileSize));

    if (!looksLikeZip(archivePath))
        return Result<void>::fail(ErrorCode::ZipInvalid, QStringLiteral("ZIP 시그니처가 아닙니다: %1").arg(archivePath));

    auto impl = std::make_unique<Impl>();
    const QByteArray nativePath = QFile::encodeName(archivePath);
    if (!mz_zip_reader_init_file(&impl->archive, nativePath.constData(), 0)) {
        return Result<void>::fail(ErrorCode::ZipInvalid,
                                  QStringLiteral("ZIP 중앙 디렉터리를 읽을 수 없습니다: %1").arg(archivePath));
    }
    impl->open = true;

    const mz_uint count = mz_zip_reader_get_num_files(&impl->archive);
    if (static_cast<int>(count) > limits.maxEntries) {
        mz_zip_reader_end(&impl->archive);
        return Result<void>::fail(ErrorCode::TooManyEntries,
                                  QStringLiteral("엔트리 수 초과: %1").arg(count));
    }

    QSet<QString> seen;

    for (mz_uint index = 0; index < count; ++index) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&impl->archive, index, &stat)) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::ZipInvalid, QStringLiteral("엔트리 정보를 읽을 수 없습니다 (index %1)").arg(index));
        }

        std::array<char, MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE> nameBuffer{};
        if (!mz_zip_reader_get_filename(&impl->archive, index, nameBuffer.data(), nameBuffer.size())) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::ZipInvalid, QStringLiteral("엔트리 이름을 읽을 수 없습니다 (index %1)").arg(index));
        }

        const QByteArray rawName(nameBuffer.data());
        const QString name = QString::fromUtf8(rawName);
        if (name.toUtf8() != rawName) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::FilenameEncoding,
                                      QStringLiteral("엔트리 이름이 UTF-8이 아닙니다: %1").arg(QString::fromLatin1(rawName)));
        }

        ZipEntry entry;
        entry.name = name;
        entry.method = stat.m_method;
        entry.compressedSize = stat.m_comp_size;
        entry.uncompressedSize = stat.m_uncomp_size;
        entry.crc32 = stat.m_crc32;
        entry.isDirectory = stat.m_is_directory != 0;
        entry.isEncrypted = stat.m_is_encrypted != 0;

        // UNIX mode lives in the high 16 bits of the external attributes.
        const quint32 unixMode = static_cast<quint32>(stat.m_external_attr >> 16);
        const quint32 fileType = unixMode & 0xF000u;
        entry.isSymlink = fileType == 0xA000u;
        if (fileType != 0 && fileType != 0x8000u && fileType != 0x4000u) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::SpecialFile,
                                      QStringLiteral("특수 파일 엔트리: %1").arg(name));
        }

        if (entry.isEncrypted) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::EncryptedEntry, QStringLiteral("암호화된 엔트리: %1").arg(name));
        }
        if (entry.isSymlink) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::SpecialFile, QStringLiteral("심볼릭 링크 엔트리: %1").arg(name));
        }
        if (entry.method != MZ_DEFLATED && entry.method != 0) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::UnsupportedCompression,
                                      QStringLiteral("허용되지 않는 압축 방식(%1): %2").arg(entry.method).arg(name));
        }
        if (!stat.m_is_supported) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::UnsupportedCompression,
                                      QStringLiteral("압축 해제를 지원하지 않는 엔트리: %1").arg(name));
        }

        auto valid = paths::validateEntryName(name, entry.isDirectory, limits.maxPathBytes);
        if (!valid) {
            mz_zip_reader_end(&impl->archive);
            return valid;
        }

        const QString key = paths::duplicationKey(name);
        if (seen.contains(key)) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::DuplicatePath, QStringLiteral("중복 경로: %1").arg(name));
        }
        seen.insert(key);

        if (entry.isDirectory) {
            impl->entries.append(entry);
            impl->indexByName.insert(key, impl->entries.size() - 1);
            continue;
        }

        if (entry.uncompressedSize > limits.maxEntrySize) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::EntryTooLarge,
                                      QStringLiteral("엔트리 크기 초과: %1 (%2 bytes)").arg(name).arg(entry.uncompressedSize));
        }

        if (entry.compressedSize > 0
            && entry.uncompressedSize / entry.compressedSize > static_cast<quint64>(limits.maxSingleEntryRatio)) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::CompressionRatio,
                                      QStringLiteral("압축비 초과: %1").arg(name));
        }

        impl->totalUncompressed += entry.uncompressedSize;
        impl->totalCompressed += entry.compressedSize;
        if (impl->totalUncompressed > limits.maxTotalSize) {
            mz_zip_reader_end(&impl->archive);
            return Result<void>::fail(ErrorCode::PackageTooLarge,
                                      QStringLiteral("전체 해제 크기 초과: %1 bytes").arg(impl->totalUncompressed));
        }

        impl->entries.append(entry);
        impl->indexByName.insert(key, impl->entries.size() - 1);
    }

    if (impl->totalCompressed > 0
        && impl->totalUncompressed / impl->totalCompressed > static_cast<quint64>(limits.maxTotalRatio)) {
        mz_zip_reader_end(&impl->archive);
        return Result<void>::fail(ErrorCode::CompressionRatio, QStringLiteral("전체 압축비 초과"));
    }

    m_impl = std::move(impl);
    return Result<void>::ok();
}

const ZipEntry* ZipReader::find(const QString& name) const
{
    if (!m_impl)
        return nullptr;
    const auto it = m_impl->indexByName.constFind(paths::duplicationKey(name));
    if (it == m_impl->indexByName.cend())
        return nullptr;
    return &m_impl->entries.at(it.value());
}

Result<QByteArray> ZipReader::read(const QString& name, quint64 maxBytes) const
{
    if (!m_impl || !m_impl->open)
        return Result<QByteArray>::fail(ErrorCode::ZipInvalid, QStringLiteral("ZIP이 열려 있지 않습니다"));

    const ZipEntry* entry = find(name);
    if (!entry)
        return Result<QByteArray>::fail(ErrorCode::EntryFileMissing, QStringLiteral("엔트리가 없습니다: %1").arg(name));
    if (entry->isDirectory)
        return Result<QByteArray>::fail(ErrorCode::EntryFileMissing, QStringLiteral("디렉터리 엔트리: %1").arg(name));
    if (entry->uncompressedSize > maxBytes)
        return Result<QByteArray>::fail(ErrorCode::EntryTooLarge, QStringLiteral("엔트리 크기 초과: %1").arg(name));

    const int index = m_impl->indexByName.value(paths::duplicationKey(name), -1);
    if (index < 0)
        return Result<QByteArray>::fail(ErrorCode::Internal, QStringLiteral("엔트리 인덱스 오류: %1").arg(name));

    QByteArray buffer;
    buffer.resize(static_cast<qsizetype>(entry->uncompressedSize));
    // mz_zip_reader_extract_to_mem returns mz_bool, not a byte count.
    const mz_bool extracted = mz_zip_reader_extract_to_mem(
        &m_impl->archive, static_cast<mz_uint>(index),
        buffer.data(), static_cast<size_t>(buffer.size()), 0);
    if (!extracted) {
        return Result<QByteArray>::fail(ErrorCode::ExtractionFailed,
                                        QStringLiteral("엔트리 압축 해제 실패: %1").arg(name));
    }
    return buffer;
}

Result<void> ZipReader::extractTo(const QString& name, const QString& destinationFile, quint64 maxBytes) const
{
    if (!m_impl || !m_impl->open)
        return Result<void>::fail(ErrorCode::ZipInvalid, QStringLiteral("ZIP이 열려 있지 않습니다"));

    const ZipEntry* entry = find(name);
    if (!entry)
        return Result<void>::fail(ErrorCode::EntryFileMissing, QStringLiteral("엔트리가 없습니다: %1").arg(name));

    const int index = m_impl->indexByName.value(paths::duplicationKey(name), -1);
    if (index < 0)
        return Result<void>::fail(ErrorCode::Internal, QStringLiteral("엔트리 인덱스 오류: %1").arg(name));

    QDir().mkpath(QFileInfo(destinationFile).absolutePath());
    QFile file(destinationFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return Result<void>::fail(ErrorCode::ExtractionFailed,
                                  QStringLiteral("대상 파일을 쓸 수 없습니다: %1").arg(destinationFile));

    ExtractContext context{&file, 0, qMin(maxBytes, entry->uncompressedSize + 1), false};
    const mz_bool ok = mz_zip_reader_extract_to_callback(
        &m_impl->archive, static_cast<mz_uint>(index), writeCallback, &context, 0);
    file.close();

    if (context.overflow) {
        QFile::remove(destinationFile);
        return Result<void>::fail(ErrorCode::EntryTooLarge,
                                  QStringLiteral("해제 중 크기 초과: %1").arg(name));
    }
    if (!ok || context.written != entry->uncompressedSize) {
        QFile::remove(destinationFile);
        return Result<void>::fail(ErrorCode::ExtractionFailed,
                                  QStringLiteral("엔트리 압축 해제 실패: %1").arg(name));
    }
    return Result<void>::ok();
}

} // namespace napp::ipkg
