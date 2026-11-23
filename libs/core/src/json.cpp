#include "napp/core/json.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace napp::json {

Result<QJsonObject> parseObject(const QByteArray& bytes, ErrorCode syntaxCode, const QString& origin)
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return Result<QJsonObject>::fail(syntaxCode,
                                         QStringLiteral("JSON 문법 오류: %1 (%2)").arg(origin, parseError.errorString()));
    if (!document.isObject())
        return Result<QJsonObject>::fail(syntaxCode, QStringLiteral("JSON 최상위가 객체가 아닙니다: %1").arg(origin));
    return document.object();
}

Result<QByteArray> readFile(const QString& path, ErrorCode failureCode)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return Result<QByteArray>::fail(failureCode, QStringLiteral("파일을 열 수 없습니다: %1").arg(path));
    return file.readAll();
}

Result<QByteArray> readFileLimited(const QString& path, qint64 maxBytes, ErrorCode failureCode)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return Result<QByteArray>::fail(failureCode, QStringLiteral("파일을 열 수 없습니다: %1").arg(path));

    const qint64 size = file.size();
    if (size > maxBytes) {
        return Result<QByteArray>::fail(failureCode,
                                        QStringLiteral("파일 크기 초과: %1 (%2 bytes)").arg(path).arg(size));
    }
    return file.readAll();
}

Result<void> writeFileAtomically(const QString& path, const QByteArray& bytes, ErrorCode failureCode)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return Result<void>::fail(failureCode, QStringLiteral("파일을 쓸 수 없습니다: %1").arg(path));
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return Result<void>::fail(failureCode, QStringLiteral("파일 쓰기 실패: %1").arg(path));
    }
    if (!file.commit())
        return Result<void>::fail(failureCode, QStringLiteral("파일 커밋 실패: %1").arg(path));
    return Result<void>::ok();
}

Result<void> writeObjectAtomically(const QString& path, const QJsonObject& object)
{
    return writeFileAtomically(path, QJsonDocument(object).toJson(QJsonDocument::Indented));
}

Result<QJsonObject> readObjectOrEmpty(const QString& path)
{
    if (!QFileInfo::exists(path))
        return QJsonObject{};
    auto bytes = readFile(path);
    if (!bytes)
        return bytes.error();
    return parseObject(bytes.value(), ErrorCode::InvalidDocument, path);
}

} // namespace napp::json
