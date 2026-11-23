#pragma once

#include "napp/core/result.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace napp::json {

// Parses an object document; `syntaxCode` distinguishes package metadata
// errors (E1004) from repository document errors.
Result<QJsonObject> parseObject(const QByteArray& bytes, ErrorCode syntaxCode, const QString& origin);

Result<QByteArray> readFile(const QString& path, ErrorCode failureCode = ErrorCode::Internal);
Result<QByteArray> readFileLimited(const QString& path, qint64 maxBytes,
                                   ErrorCode failureCode = ErrorCode::ResponseTooLarge);

// Writes via "<path>.tmp" then rename, so a crash never leaves a partial file.
Result<void> writeFileAtomically(const QString& path, const QByteArray& bytes,
                                 ErrorCode failureCode = ErrorCode::DiskError);

Result<void> writeObjectAtomically(const QString& path, const QJsonObject& object);

// Reads a JSON object from disk; returns an empty object when missing
// (callers treat "missing" as "no data yet").
Result<QJsonObject> readObjectOrEmpty(const QString& path);

} // namespace napp::json
