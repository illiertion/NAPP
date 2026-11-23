#pragma once

#include "napp/core/paths.h"
#include "napp/core/result.h"

#include <QDate>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <memory>

namespace napp::store {

class HttpClient;
class CatalogCache;

struct RepositoryConfig
{
    QString id;
    QString name;
    QString baseUrl; // e.g. https://raw.githubusercontent.com/ILLIERTION/NAPP-Repository/main/
    bool enabled = true;
    bool official = false;

    QString host() const { return QUrl(baseUrl).host(); }
    QUrl resolve(const QString& relative) const;
};

struct AppVersionInfo
{
    QString version;
    QDate releaseDate;
    QString changelog;
    QUrl url;
    QString sha256;
    qint64 size = 0;
    QStringList platforms;
    bool yanked = false;
    bool deprecated = false;
};

struct CatalogApp
{
    QString id;
    QString name;
    QString description;
    QString author;
    QString category;
    QString iconUrl;
    QString homepage;
    QString license;
    QString repositoryId;
    QString manifestPath;
    QStringList platforms;
    QMap<QString, AppVersionInfo> versions;
    QString latest;
    QStringList warnings;

    bool isValid() const { return !id.isEmpty() && !latest.isEmpty() && versions.contains(latest); }

    // Newest non-yanked version that supports `platform`, empty when none.
    QString bestVersionFor(const QString& platform) const;
    const AppVersionInfo* versionInfo(const QString& version) const;
};

// Loads repository.json and the per-app JSON documents, with TTL caching and
// offline fallback. All work is asynchronous; the UI only sees signals.
class RepositoryService : public QObject
{
    Q_OBJECT

public:
    RepositoryService(Paths paths, HttpClient* http, QObject* parent = nullptr);
    ~RepositoryService() override;

    const QList<RepositoryConfig>& repositories() const { return m_repositories; }
    const QList<CatalogApp>& catalog() const { return m_catalog; }

    Result<void> loadRepositories();
    Result<void> saveRepositories() const;
    Result<void> addRepository(const QString& name, const QString& baseUrl);
    Result<void> removeRepository(const QString& id);
    void setRepositoryEnabled(const QString& id, bool enabled);

    // Fetches all enabled repositories (cache first when fresh, network next).
    void refresh();

    const CatalogApp* findApp(const QString& appId) const;
    QString repositoryName(const QString& repositoryId) const;

    // Base URL of the first enabled repository. Apps receive it as
    // window.napp.repositoryUrl so docs/content can be fetched from the store
    // instead of being bundled into every package.
    QString primaryBaseUrl() const;

    static qint64 documentTtlSeconds() { return 6 * 60 * 60; }

signals:
    void catalogChanged();
    void refreshStarted();
    void refreshFinished();
    void repositoryFailed(const QString& repositoryId, const QString& message);
    void statusMessage(const QString& message);

private:
    void refreshRepository(int index);
    void fetchAppDocument(const RepositoryConfig& repository, const QString& appId, const QString& manifestPath);
    void fetchDocument(const QUrl& url, const QString& allowHost,
                       std::function<void(const QJsonObject&)> onDocument,
                       std::function<void(const QString&)> onFailure);
    CatalogApp parseAppDocument(const RepositoryConfig& repository, const QString& manifestPath,
                                const QJsonObject& document, QStringList& errors) const;

    Paths m_paths;
    HttpClient* m_http = nullptr;
    std::unique_ptr<CatalogCache> m_cache;
    QList<RepositoryConfig> m_repositories;
    QList<CatalogApp> m_catalog;
    int m_pendingRepositories = 0;
    bool m_refreshing = false;
};

} // namespace napp::store

Q_DECLARE_METATYPE(napp::store::CatalogApp)
