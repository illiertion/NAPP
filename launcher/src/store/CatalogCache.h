#pragma once

#include "napp/core/result.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>

namespace napp::store {

// File based cache for repository documents and icons. The launcher keeps the
// last good copy so an offline start still shows the store (spec §9.1).
class CatalogCache
{
public:
    explicit CatalogCache(QString cacheDir);

    // Returns a cached document when it is younger than ttlSeconds.
    Result<QJsonObject> load(const QString& url, qint64 ttlSeconds) const;

    // Returns the cached document regardless of age (offline fallback).
    Result<QJsonObject> loadAny(const QString& url) const;

    Result<void> save(const QString& url, const QJsonObject& document);

    QDateTime cachedAt(const QString& url) const;

    // Downloads-free icon cache path for a URL (empty when nothing cached).
    QString iconPath(const QString& url) const;

private:
    QString entryPath(const QString& url, const char* suffix) const;

    QString m_cacheDir;
};

} // namespace napp::store
