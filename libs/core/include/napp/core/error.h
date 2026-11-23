#pragma once

#include <QString>

namespace napp {

// Failure codes used across the launcher, IPKG library and runtime host.
// The specCode() mapping mirrors specifications/IPKG_SPECIFICATION.md §6.
enum class ErrorCode {
    None = 0,

    // Container / archive
    ZipInvalid,            // E1001
    UnsupportedCompression,// E1002
    ManifestMissing,       // E1003
    ManifestSyntax,        // E1004
    ManifestSchema,        // E1005
    HashMismatch,          // E1006
    EncryptedEntry,        // E1007
    FilenameEncoding,      // E1009
    UnsafePath,            // E1010
    SpecialFile,           // E1011
    DuplicatePath,         // E1012
    TooManyEntries,        // E1013
    EntryTooLarge,         // E1014
    PackageTooLarge,       // E1015
    CompressionRatio,      // E1016
    ExtractionFailed,      // E1017
    EntryFileMissing,      // E1018
    MetadataFileMissing,   // E1019
    UnsupportedValue,      // E1020
    RepositoryMismatch,    // E1021
    UnsupportedPlatform,   // E1022
    SignatureInvalid,      // E1023

    // Network / repository
    NetworkError,
    HttpStatus,
    ResponseTooLarge,
    RedirectLimit,
    HostNotAllowed,
    InvalidDocument,

    // Installation
    AppAlreadyInstalled,
    AppNotFound,
    DiskError,
    RuntimeMissing,
    RuntimeUnsupported,
    InstallBusy,

    Internal
};

struct Error {
    ErrorCode code = ErrorCode::None;
    QString message;
    QString detail;

    bool isNone() const { return code == ErrorCode::None; }

    static Error none() { return {}; }
    static Error make(ErrorCode code, const QString& message, const QString& detail = {});
};

// Machine readable spec code such as "E1010"; empty for non-spec errors.
QString specCode(ErrorCode code);

// Stable identifier for logging and tests, e.g. "UnsafePath".
QString errorCodeName(ErrorCode code);

} // namespace napp
