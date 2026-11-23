#include "CatalogCache.h"

#include "napp/core/json.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>

namespace napp::store {

CatalogCache::CatalogCache(QString cacheDir)
    : m_cacheDir(std::move(cacheDir))
{
    QDir().mkpath(QDir(m_cacheDir).filePath(QStringLiteral("documents")));
    QDir().mkpath(QDir(m_cacheDir).filePath(QStringLiteral("icons")));
}

QString CatalogCache::entryPath(const QString& url, const char* suffix) const
{
    const QByteArray key =
        QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha256).toHex().left(40);
    return QDir(m_cacheDir).filePath(QStringLiteral("%1/%2.%3")
                                         .arg(QString::fromLatin1(suffix), QString::fromLatin1(key),
                                              QString::fromLatin1(suffix)));
}

Result<QJsonObject> CatalogCache::load(const QString& url, qint64 ttlSeconds) const
{
    const QString path = entryPath(url, "documents");
    const QFileInfo info(path);
    if (!info.exists())
        return Result<QJsonObject>::fail(ErrorCode::InvalidDocument, QStringLiteral("캐시 없음"));

    if (info.lastModified().secsTo(QDateTime::currentDateTime()) > ttlSeconds)
        return Result<QJsonObject>::fail(ErrorCode::InvalidDocument, QStringLiteral("캐시 만료"));

    return json::readObjectOrEmpty(path);
}

Result<QJsonObject> CatalogCache::loadAny(const QString& url) const
{
    const QString path = entryPath(url, "documents");
    if (!QFileInfo::exists(path))
        return Result<QJsonObject>::fail(ErrorCode::InvalidDocument, QStringLiteral("캐시 없음"));
    return json::readObjectOrEmpty(path);
}

Result<void> CatalogCache::save(const QString& url, const QJsonObject& document)
{
    return json::writeObjectAtomically(entryPath(url, "documents"), document);
}

QDateTime CatalogCache::cachedAt(const QString& url) const
{
    return QFileInfo(entryPath(url, "documents")).lastModified();
}

QString CatalogCache::iconPath(const QString& url) const
{
    const QString path = entryPath(url, "icons");
    return QFileInfo::exists(path) ? path : QString();
}

} // namespace napp::store
