#include "napp/core/error.h"

#include <QHash>

namespace napp {

Error Error::make(ErrorCode code, const QString& message, const QString& detail)
{
    return Error{code, message, detail};
}

QString specCode(ErrorCode code)
{
    switch (code) {
    case ErrorCode::ZipInvalid: return QStringLiteral("E1001");
    case ErrorCode::UnsupportedCompression: return QStringLiteral("E1002");
    case ErrorCode::ManifestMissing: return QStringLiteral("E1003");
    case ErrorCode::ManifestSyntax: return QStringLiteral("E1004");
    case ErrorCode::ManifestSchema: return QStringLiteral("E1005");
    case ErrorCode::HashMismatch: return QStringLiteral("E1006");
    case ErrorCode::EncryptedEntry: return QStringLiteral("E1007");
    case ErrorCode::FilenameEncoding: return QStringLiteral("E1009");
    case ErrorCode::UnsafePath: return QStringLiteral("E1010");
    case ErrorCode::SpecialFile: return QStringLiteral("E1011");
    case ErrorCode::DuplicatePath: return QStringLiteral("E1012");
    case ErrorCode::TooManyEntries: return QStringLiteral("E1013");
    case ErrorCode::EntryTooLarge: return QStringLiteral("E1014");
    case ErrorCode::PackageTooLarge: return QStringLiteral("E1015");
    case ErrorCode::CompressionRatio: return QStringLiteral("E1016");
    case ErrorCode::ExtractionFailed: return QStringLiteral("E1017");
    case ErrorCode::EntryFileMissing: return QStringLiteral("E1018");
    case ErrorCode::MetadataFileMissing: return QStringLiteral("E1019");
    case ErrorCode::UnsupportedValue: return QStringLiteral("E1020");
    case ErrorCode::RepositoryMismatch: return QStringLiteral("E1021");
    case ErrorCode::UnsupportedPlatform: return QStringLiteral("E1022");
    case ErrorCode::SignatureInvalid: return QStringLiteral("E1023");
    default: return {};
    }
}

QString errorCodeName(ErrorCode code)
{
    switch (code) {
    case ErrorCode::None: return QStringLiteral("None");
    case ErrorCode::ZipInvalid: return QStringLiteral("ZipInvalid");
    case ErrorCode::UnsupportedCompression: return QStringLiteral("UnsupportedCompression");
    case ErrorCode::ManifestMissing: return QStringLiteral("ManifestMissing");
    case ErrorCode::ManifestSyntax: return QStringLiteral("ManifestSyntax");
    case ErrorCode::ManifestSchema: return QStringLiteral("ManifestSchema");
    case ErrorCode::HashMismatch: return QStringLiteral("HashMismatch");
    case ErrorCode::EncryptedEntry: return QStringLiteral("EncryptedEntry");
    case ErrorCode::FilenameEncoding: return QStringLiteral("FilenameEncoding");
    case ErrorCode::UnsafePath: return QStringLiteral("UnsafePath");
    case ErrorCode::SpecialFile: return QStringLiteral("SpecialFile");
    case ErrorCode::DuplicatePath: return QStringLiteral("DuplicatePath");
    case ErrorCode::TooManyEntries: return QStringLiteral("TooManyEntries");
    case ErrorCode::EntryTooLarge: return QStringLiteral("EntryTooLarge");
    case ErrorCode::PackageTooLarge: return QStringLiteral("PackageTooLarge");
    case ErrorCode::CompressionRatio: return QStringLiteral("CompressionRatio");
    case ErrorCode::ExtractionFailed: return QStringLiteral("ExtractionFailed");
    case ErrorCode::EntryFileMissing: return QStringLiteral("EntryFileMissing");
    case ErrorCode::MetadataFileMissing: return QStringLiteral("MetadataFileMissing");
    case ErrorCode::UnsupportedValue: return QStringLiteral("UnsupportedValue");
    case ErrorCode::RepositoryMismatch: return QStringLiteral("RepositoryMismatch");
    case ErrorCode::UnsupportedPlatform: return QStringLiteral("UnsupportedPlatform");
    case ErrorCode::SignatureInvalid: return QStringLiteral("SignatureInvalid");
    case ErrorCode::NetworkError: return QStringLiteral("NetworkError");
    case ErrorCode::HttpStatus: return QStringLiteral("HttpStatus");
    case ErrorCode::ResponseTooLarge: return QStringLiteral("ResponseTooLarge");
    case ErrorCode::RedirectLimit: return QStringLiteral("RedirectLimit");
    case ErrorCode::HostNotAllowed: return QStringLiteral("HostNotAllowed");
    case ErrorCode::InvalidDocument: return QStringLiteral("InvalidDocument");
    case ErrorCode::AppAlreadyInstalled: return QStringLiteral("AppAlreadyInstalled");
    case ErrorCode::AppNotFound: return QStringLiteral("AppNotFound");
    case ErrorCode::DiskError: return QStringLiteral("DiskError");
    case ErrorCode::RuntimeMissing: return QStringLiteral("RuntimeMissing");
    case ErrorCode::RuntimeUnsupported: return QStringLiteral("RuntimeUnsupported");
    case ErrorCode::InstallBusy: return QStringLiteral("InstallBusy");
    case ErrorCode::Internal: return QStringLiteral("Internal");
    }
    return QStringLiteral("Unknown");
}

} // namespace napp
