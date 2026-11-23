#include "napp/ipkg/package_verifier.h"

#include "napp/core/error.h"
#include "napp/core/platform.h"
#include "napp/ipkg/manifest_parser.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace napp::ipkg {

PackageVerifier::PackageVerifier(Limits limits)
    : m_limits(limits)
{
}

bool PackageVerifier::isValidSha256(const QString& value)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-f0-9]{64}$"));
    return pattern.match(value).hasMatch();
}

QString PackageVerifier::computeSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

Result<PackageInfo> PackageVerifier::verify(const QString& packagePath, const QString& expectedSha256) const
{
    const QFileInfo info(packagePath);
    if (!info.exists() || !info.isFile())
        return Result<PackageInfo>::fail(ErrorCode::ZipInvalid, QStringLiteral("패키지 파일이 없습니다: %1").arg(packagePath));

    if (!expectedSha256.isEmpty() && !isValidSha256(expectedSha256))
        return Result<PackageInfo>::fail(ErrorCode::HashMismatch, QStringLiteral("저장소의 SHA-256 값 형식이 올바르지 않습니다"));

    PackageInfo package;
    package.packagePath = info.absoluteFilePath();
    package.packageSize = info.size();

    if (!expectedSha256.isEmpty()) {
        const QString actual = computeSha256(packagePath);
        if (actual.isEmpty())
            return Result<PackageInfo>::fail(ErrorCode::DiskError, QStringLiteral("패키지를 읽을 수 없습니다: %1").arg(packagePath));
        if (actual != expectedSha256) {
            return Result<PackageInfo>::fail(
                ErrorCode::HashMismatch,
                QStringLiteral("SHA-256 검증 실패"),
                QStringLiteral("expected=%1 actual=%2").arg(expectedSha256, actual));
        }
        package.sha256 = actual;
    } else {
        package.sha256 = computeSha256(packagePath);
    }

    ZipReader reader;
    auto opened = reader.open(packagePath, m_limits);
    if (!opened)
        return opened.error();

    if (!reader.find(QStringLiteral("manifest.json")))
        return Result<PackageInfo>::fail(ErrorCode::ManifestMissing, QStringLiteral("패키지 루트에 manifest.json 이 없습니다"));

    auto manifest = ManifestParser::readFrom(reader);
    if (!manifest)
        return manifest.error();

    if (!reader.find(manifest.value().entry)) {
        return Result<PackageInfo>::fail(ErrorCode::EntryFileMissing,
                                         QStringLiteral("entry 파일이 패키지에 없습니다: %1").arg(manifest.value().entry));
    }
    if (!reader.find(manifest.value().iconPath)) {
        return Result<PackageInfo>::fail(ErrorCode::MetadataFileMissing,
                                         QStringLiteral("아이콘 파일이 패키지에 없습니다: %1").arg(manifest.value().iconPath));
    }
    if (!reader.find(manifest.value().previewPath)) {
        return Result<PackageInfo>::fail(ErrorCode::MetadataFileMissing,
                                         QStringLiteral("미리보기 파일이 패키지에 없습니다: %1").arg(manifest.value().previewPath));
    }
    for (const QString& screenshot : manifest.value().screenshots) {
        if (!reader.find(screenshot)) {
            return Result<PackageInfo>::fail(ErrorCode::MetadataFileMissing,
                                             QStringLiteral("스크린샷이 패키지에 없습니다: %1").arg(screenshot));
        }
    }

    package.manifest = manifest.value();
    package.entries = reader.entries();
    return package;
}

Result<void> PackageVerifier::checkPlatform(const Manifest& manifest) const
{
    const QString current = currentPlatformId();
    if (!manifest.platforms.contains(current)) {
        QStringList supported;
        for (const QString& platform : manifest.platforms)
            supported.append(platformDisplayName(platform));
        return Result<void>::fail(
            ErrorCode::UnsupportedPlatform,
            QStringLiteral("이 앱은 현재 플랫폼(%1)을 지원하지 않습니다").arg(platformDisplayName(current)),
            QStringLiteral("지원 플랫폼: %1").arg(supported.join(QStringLiteral(", "))));
    }
    return Result<void>::ok();
}

} // namespace napp::ipkg
