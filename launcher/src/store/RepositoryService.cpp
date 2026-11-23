#include "RepositoryService.h"

#include "CatalogCache.h"
#include "napp/core/json.h"
#include "napp/core/logging.h"
#include "napp/core/platform.h"
#include "napp/core/semver.h"
#include "network/HttpClient.h"

#include <QJsonArray>
#include <QSet>

namespace napp::store {
namespace {

#ifndef NAPP_OFFICIAL_REPOSITORY_URL
#define NAPP_OFFICIAL_REPOSITORY_URL "https://raw.githubusercontent.com/ILLIERTION/NAPP-Repository/main/"
#endif

constexpr auto kOfficialBaseUrl = NAPP_OFFICIAL_REPOSITORY_URL;

QStringList platformList(const QJsonObject& object, const QString& key)
{
    QStringList platforms;
    for (const QJsonValue& value : object.value(key).toArray()) {
        if (value.isString())
            platforms.append(value.toString());
    }
    return platforms;
}

} // namespace

QUrl RepositoryConfig::resolve(const QString& relative) const
{
    if (relative.startsWith(QStringLiteral("https://")))
        return QUrl(relative);
    QString base = baseUrl;
    if (!base.endsWith(QLatin1Char('/')))
        base += QLatin1Char('/');
    return QUrl(base + relative);
}

QString CatalogApp::bestVersionFor(const QString& platform) const
{
    QString best;
    for (auto it = versions.constBegin(); it != versions.constEnd(); ++it) {
        const AppVersionInfo& info = it.value();
        if (info.yanked)
            continue;
        const QStringList supported = info.platforms.isEmpty() ? platforms : info.platforms;
        if (!platform.isEmpty() && !supported.contains(platform))
            continue;
        if (best.isEmpty()) {
            best = it.key();
            continue;
        }
        if (const auto comparison = SemVer::compareStrings(it.key(), best); comparison && comparison.value() > 0)
            best = it.key();
    }
    return best;
}

const AppVersionInfo* CatalogApp::versionInfo(const QString& version) const
{
    const auto it = versions.constFind(version);
    return it == versions.constEnd() ? nullptr : &it.value();
}

RepositoryService::RepositoryService(Paths paths, HttpClient* http, QObject* parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_http(http)
    , m_cache(std::make_unique<CatalogCache>(m_paths.cacheDir()))
{
}

RepositoryService::~RepositoryService() = default;

Result<void> RepositoryService::loadRepositories()
{
    auto document = json::readObjectOrEmpty(m_paths.repositoriesFile());
    if (!document)
        return document.error();

    m_repositories.clear();
    const QJsonArray repositories = document.value().value(QStringLiteral("repositories")).toArray();
    for (const QJsonValue& value : repositories) {
        const QJsonObject object = value.toObject();
        RepositoryConfig config;
        config.id = object.value(QStringLiteral("id")).toString();
        config.name = object.value(QStringLiteral("name")).toString();
        config.baseUrl = object.value(QStringLiteral("base_url")).toString();
        config.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        config.official = object.value(QStringLiteral("official")).toBool(false);
        if (!config.id.isEmpty() && !config.baseUrl.isEmpty())
            m_repositories.append(config);
    }

    if (m_repositories.isEmpty()) {
        RepositoryConfig official;
        official.id = QStringLiteral("com.illiertion.official");
        official.name = QStringLiteral("ILLIERTION Official Repository");
        official.baseUrl = QString::fromLatin1(kOfficialBaseUrl);
        official.official = true;
        m_repositories.append(official);
        auto saved = saveRepositories();
        if (!saved)
            qWarning("기본 저장소를 저장하지 못했습니다: %s", qPrintable(saved.error().message));
    }

    return Result<void>::ok();
}

Result<void> RepositoryService::saveRepositories() const
{
    QJsonArray array;
    for (const RepositoryConfig& repository : m_repositories) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), repository.id);
        object.insert(QStringLiteral("name"), repository.name);
        object.insert(QStringLiteral("base_url"), repository.baseUrl);
        object.insert(QStringLiteral("enabled"), repository.enabled);
        object.insert(QStringLiteral("official"), repository.official);
        array.append(object);
    }

    QJsonObject document;
    document.insert(QStringLiteral("schema_version"), 1);
    document.insert(QStringLiteral("repositories"), array);
    return json::writeObjectAtomically(m_paths.repositoriesFile(), document);
}

Result<void> RepositoryService::addRepository(const QString& name, const QString& baseUrl)
{
    const QUrl url(baseUrl);
    if (!url.isValid() || url.scheme() != QStringLiteral("https")) {
        return Result<void>::fail(ErrorCode::InvalidDocument,
                                  QStringLiteral("저장소 URL은 https여야 합니다"));
    }
    if (name.trimmed().isEmpty())
        return Result<void>::fail(ErrorCode::InvalidDocument, QStringLiteral("저장소 이름이 비어 있습니다"));

    for (const RepositoryConfig& repository : m_repositories) {
        if (QUrl(repository.baseUrl).host() == url.host() && repository.baseUrl == baseUrl) {
            return Result<void>::fail(ErrorCode::InvalidDocument,
                                      QStringLiteral("이미 등록된 저장소입니다"));
        }
    }

    RepositoryConfig config;
    config.id = QStringLiteral("custom.") + QUrl(baseUrl).host().toLower() + QLatin1Char('.')
        + QString::number(m_repositories.size());
    config.name = name.trimmed();
    config.baseUrl = baseUrl.endsWith(QLatin1Char('/')) ? baseUrl : baseUrl + QLatin1Char('/');
    m_repositories.append(config);

    auto saved = saveRepositories();
    if (!saved) {
        m_repositories.removeLast();
        return saved.error();
    }
    return Result<void>::ok();
}

Result<void> RepositoryService::removeRepository(const QString& id)
{
    for (int index = 0; index < m_repositories.size(); ++index) {
        if (m_repositories.at(index).id != id)
            continue;
        if (m_repositories.at(index).official) {
            return Result<void>::fail(ErrorCode::InvalidDocument,
                                      QStringLiteral("공식 저장소는 삭제할 수 없습니다(비활성화만 가능)"));
        }
        const RepositoryConfig removed = m_repositories.takeAt(index);
        auto saved = saveRepositories();
        if (!saved) {
            m_repositories.insert(index, removed);
            return saved.error();
        }
        return Result<void>::ok();
    }
    return Result<void>::fail(ErrorCode::InvalidDocument, QStringLiteral("저장소를 찾을 수 없습니다"));
}

void RepositoryService::setRepositoryEnabled(const QString& id, bool enabled)
{
    for (RepositoryConfig& repository : m_repositories) {
        if (repository.id == id) {
            repository.enabled = enabled;
            saveRepositories();
            return;
        }
    }
}

const CatalogApp* RepositoryService::findApp(const QString& appId) const
{
    for (const CatalogApp& app : m_catalog) {
        if (app.id == appId)
            return &app;
    }
    return nullptr;
}

QString RepositoryService::repositoryName(const QString& repositoryId) const
{
    for (const RepositoryConfig& repository : m_repositories) {
        if (repository.id == repositoryId)
            return repository.name;
    }
    return repositoryId;
}

QString RepositoryService::primaryBaseUrl() const
{
    for (const RepositoryConfig& repository : m_repositories) {
        if (repository.enabled && !repository.baseUrl.isEmpty())
            return repository.baseUrl;
    }
    return {};
}

void RepositoryService::refresh()
{
    if (m_refreshing) {
        emit statusMessage(QStringLiteral("저장소 갱신이 이미 진행 중입니다"));
        return;
    }

    m_refreshing = true;
    m_catalog.clear();
    emit refreshStarted();

    m_pendingRepositories = 0;
    for (int index = 0; index < m_repositories.size(); ++index) {
        if (!m_repositories.at(index).enabled)
            continue;
        ++m_pendingRepositories;
        refreshRepository(index);
    }

    if (m_pendingRepositories == 0) {
        m_refreshing = false;
        emit catalogChanged();
        emit refreshFinished();
    }
}

void RepositoryService::refreshRepository(int index)
{
    const RepositoryConfig repository = m_repositories.at(index);
    const QUrl indexUrl = repository.resolve(QStringLiteral("repository.json"));
    const QString allowHost = repository.host();
    QStringList allowed = HttpClient::defaultAllowedHosts();
    if (!allowHost.isEmpty() && !allowed.contains(allowHost, Qt::CaseInsensitive))
        allowed.append(allowHost);

    auto finishRepository = [this] {
        if (--m_pendingRepositories <= 0) {
            m_refreshing = false;
            emit catalogChanged();
            emit refreshFinished();
        }
    };

    auto handleIndex = [this, repository, allowed, finishRepository](const QJsonObject& document,
                                                                    bool fromCache) {
        if (document.value(QStringLiteral("format")).toString() != QStringLiteral("NAPP-REPOSITORY")) {
            emit repositoryFailed(repository.id, QStringLiteral("repository.json 형식이 올바르지 않습니다"));
            m_pendingRepositories--; // this repository contributes no apps
            if (m_pendingRepositories <= 0) {
                m_refreshing = false;
                emit catalogChanged();
                emit refreshFinished();
            }
            return;
        }

        struct Pending
        {
            QString appId;
            QString manifestPath;
        };
        QList<Pending> pending;
        for (const QJsonValue& value : document.value(QStringLiteral("apps")).toArray()) {
            const QJsonObject entry = value.toObject();
            const QString appId = entry.value(QStringLiteral("id")).toString();
            const QString manifestPath = entry.value(QStringLiteral("manifest")).toString();
            if (!appId.isEmpty() && !manifestPath.isEmpty())
                pending.append({appId, manifestPath});
        }

        if (fromCache)
            emit statusMessage(QStringLiteral("%1: 캐시를 사용했습니다").arg(repository.name));

        for (const Pending& item : pending)
            fetchAppDocument(repository, item.appId, item.manifestPath);

        finishRepository();
    };

    // Cache first when fresh, so launching again is offline-friendly.
    if (auto cached = m_cache->load(indexUrl.toString(), documentTtlSeconds()); cached.isOk()) {
        handleIndex(cached.value(), true);
        return;
    }

    HttpOptions options;
    options.allowedHosts = allowed;
    m_http->getJson(
        indexUrl, options,
        [this, indexUrl, repository, allowed, handleIndex, finishRepository](Result<QJsonObject> result) {
            if (result.isOk()) {
                m_cache->save(indexUrl.toString(), result.value());
                handleIndex(result.value(), false);
                return;
            }

            if (auto offline = m_cache->loadAny(indexUrl.toString()); offline.isOk()) {
                emit repositoryFailed(repository.id,
                                      QStringLiteral("네트워크 오류로 캐시를 사용합니다: %1").arg(result.error().message));
                handleIndex(offline.value(), true);
                return;
            }

            emit repositoryFailed(repository.id,
                                  QStringLiteral("저장소를 불러오지 못했습니다: %1").arg(result.error().message));
            finishRepository();
        });
}

void RepositoryService::fetchAppDocument(const RepositoryConfig& repository, const QString& appId,
                                         const QString& manifestPath)
{
    const QUrl url = repository.resolve(manifestPath);
    QStringList allowed = HttpClient::defaultAllowedHosts();
    const QString host = url.host();
    if (!host.isEmpty() && !allowed.contains(host, Qt::CaseInsensitive))
        allowed.append(host);

    auto handle = [this, repository, appId, manifestPath](const QJsonObject& document) {
        QStringList errors;
        const CatalogApp app = parseAppDocument(repository, manifestPath, document, errors);
        if (!app.id.isEmpty()) {
            m_catalog.append(app);
            for (const QString& warning : errors)
                qWarning("catalog %s: %s", qPrintable(app.id), qPrintable(warning));
            emit catalogChanged();
        } else {
            emit repositoryFailed(repository.id,
                                  QStringLiteral("%1: 앱 문서가 올바르지 않습니다").arg(appId));
        }
    };

    if (auto cached = m_cache->load(url.toString(), documentTtlSeconds()); cached.isOk()) {
        handle(cached.value());
        return;
    }

    HttpOptions options;
    options.allowedHosts = allowed;
    m_http->getJson(url, options, [this, url, appId, repository, handle](Result<QJsonObject> result) {
        if (result.isOk()) {
            m_cache->save(url.toString(), result.value());
            handle(result.value());
            return;
        }
        if (auto offline = m_cache->loadAny(url.toString()); offline.isOk()) {
            handle(offline.value());
            return;
        }
        emit repositoryFailed(repository.id,
                              QStringLiteral("%1: 앱 정보를 불러오지 못했습니다 (%2)")
                                  .arg(appId, result.error().message));
    });
}

CatalogApp RepositoryService::parseAppDocument(const RepositoryConfig& repository,
                                               const QString& manifestPath,
                                               const QJsonObject& document,
                                               QStringList& errors) const
{
    CatalogApp app;
    if (document.value(QStringLiteral("format")).toString() != QStringLiteral("NAPP-APP")) {
        errors.append(QStringLiteral("format != NAPP-APP"));
        return app;
    }

    const QJsonObject appObject = document.value(QStringLiteral("app")).toObject();
    app.id = appObject.value(QStringLiteral("id")).toString();
    app.name = appObject.value(QStringLiteral("name")).toString();
    app.description = appObject.value(QStringLiteral("description")).toString();
    app.author = appObject.value(QStringLiteral("author")).toString();
    app.category = appObject.value(QStringLiteral("category")).toString();
    app.iconUrl = appObject.value(QStringLiteral("icon")).toString();
    app.homepage = appObject.value(QStringLiteral("homepage")).toString();
    app.license = appObject.value(QStringLiteral("license")).toString();
    app.repositoryId = repository.id;
    app.manifestPath = manifestPath;

    const QJsonObject compatibility = document.value(QStringLiteral("compatibility")).toObject();
    app.platforms = platformList(compatibility, QStringLiteral("platforms"));
    if (app.platforms.isEmpty())
        app.platforms = platformList(document.value(QStringLiteral("app")).toObject(), QStringLiteral("platforms"));

    const QJsonObject release = document.value(QStringLiteral("release")).toObject();
    app.latest = release.value(QStringLiteral("latest")).toString();

    const QJsonObject versions = release.value(QStringLiteral("versions")).toObject();
    for (auto it = versions.constBegin(); it != versions.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        const QJsonObject package = entry.value(QStringLiteral("package")).toObject();

        AppVersionInfo info;
        info.version = it.key();
        if (!SemVer::isValid(info.version)) {
            errors.append(QStringLiteral("잘못된 버전 키: %1").arg(info.version));
            continue;
        }
        info.releaseDate = QDate::fromString(entry.value(QStringLiteral("release_date")).toString(), Qt::ISODate);
        info.changelog = entry.value(QStringLiteral("changelog")).toString();
        info.url = repository.resolve(package.value(QStringLiteral("url")).toString());
        info.sha256 = package.value(QStringLiteral("sha256")).toString();
        info.size = static_cast<qint64>(package.value(QStringLiteral("size")).toDouble());
        info.platforms = platformList(entry, QStringLiteral("platforms"));
        info.yanked = entry.value(QStringLiteral("yanked")).toBool(false);
        info.deprecated = entry.value(QStringLiteral("deprecated")).toBool(false);

        if (package.value(QStringLiteral("type")).toString() != QStringLiteral("web"))
            errors.append(QStringLiteral("%1: package.type 이 web 이 아닙니다").arg(info.version));
        if (info.sha256.size() != 64)
            errors.append(QStringLiteral("%1: sha256 형식 오류").arg(info.version));
        if (!info.url.isValid() || info.url.scheme() != QStringLiteral("https"))
            errors.append(QStringLiteral("%1: 다운로드 URL이 https가 아닙니다").arg(info.version));

        app.versions.insert(info.version, info);
    }

    if (!app.versions.contains(app.latest))
        errors.append(QStringLiteral("release.latest(%1) 가 versions에 없습니다").arg(app.latest));

    if (!app.id.isEmpty() && app.name.isEmpty())
        errors.append(QStringLiteral("%1: 앱 이름이 없습니다").arg(app.id));

    return app;
}

void RepositoryService::fetchDocument(const QUrl& url, const QString& allowHost,
                                      std::function<void(const QJsonObject&)> onDocument,
                                      std::function<void(const QString&)> onFailure)
{
    QStringList allowed = HttpClient::defaultAllowedHosts();
    if (!allowHost.isEmpty() && !allowed.contains(allowHost, Qt::CaseInsensitive))
        allowed.append(allowHost);

    HttpOptions options;
    options.allowedHosts = allowed;
    m_http->getJson(url, options, [this, url, onDocument, onFailure](Result<QJsonObject> result) {
        if (result.isOk()) {
            m_cache->save(url.toString(), result.value());
            onDocument(result.value());
            return;
        }
        if (auto offline = m_cache->loadAny(url.toString()); offline.isOk()) {
            onDocument(offline.value());
            return;
        }
        if (onFailure)
            onFailure(result.error().message);
    });
}

} // namespace napp::store
