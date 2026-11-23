#include "InstallService.h"

#include "napp/core/logging.h"
#include "napp/ipkg/package_verifier.h"
#include "network/HttpClient.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>

namespace napp::core {

InstallService::InstallService(Paths paths, store::HttpClient* http, QObject* parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_http(http)
{
    connect(&m_watcher, &QFutureWatcher<InstallOutcome>::finished, this, [this] {
        const InstallOutcome outcome = m_watcher.result();
        if (outcome.ok)
            finishSuccess(outcome);
        else
            finishFailure(m_currentAppId, outcome.message);
    });
}

InstallService::~InstallService() = default;

bool InstallService::installFromUrl(const QString& appId, const QString& version, const QUrl& packageUrl,
                                    const QString& expectedSha256, const QString& repositoryId)
{
    if (m_busy) {
        emit failed(appId, QStringLiteral("다른 설치 작업이 진행 중입니다"));
        return false;
    }
    if (packageUrl.scheme() != QStringLiteral("https")) {
        emit failed(appId, QStringLiteral("다운로드 URL은 https여야 합니다"));
        return false;
    }
    if (!QDir().mkpath(m_paths.stagingDir())) {
        emit failed(appId, QStringLiteral("임시 디렉터리를 만들 수 없습니다"));
        return false;
    }

    m_busy = true;
    m_currentAppId = appId;
    m_currentVersion = version;
    m_downloadPath = QDir(m_paths.stagingDir()).filePath(QStringLiteral("%1-%2.ipkg.part").arg(appId, version));
    QFile::remove(m_downloadPath);

    emit started(appId, version);

    store::HttpOptions options;
    options.timeoutMs = 60000;

    m_http->download(
        packageUrl, m_downloadPath, options,
        [this, appId](qint64 received, qint64 total) { emit progress(appId, received, total); },
        [this, appId, version, expectedSha256, repositoryId](Result<void> result) {
            if (result.isError()) {
                QFile::remove(m_downloadPath);
                finishFailure(appId, result.error().message);
                return;
            }
            beginInstall(m_downloadPath, appId, version, expectedSha256, repositoryId);
        });

    return true;
}

bool InstallService::installFromFile(const QString& packagePath, const QString& repositoryId)
{
    if (m_busy) {
        emit failed(QStringLiteral("file"), QStringLiteral("다른 설치 작업이 진행 중입니다"));
        return false;
    }
    if (!QFileInfo::exists(packagePath)) {
        emit failed(QStringLiteral("file"), QStringLiteral("파일이 없습니다: %1").arg(packagePath));
        return false;
    }

    m_busy = true;
    m_currentAppId = QStringLiteral("file");
    m_currentVersion.clear();
    m_downloadPath = packagePath;
    beginInstall(packagePath, {}, {}, {}, repositoryId);
    return true;
}

void InstallService::beginInstall(const QString& packagePath, const QString& appId, const QString& version,
                                  const QString& expectedSha256, const QString& repositoryId)
{
    const Paths paths = m_paths;
    m_watcher.setFuture(QtConcurrent::run([paths, packagePath, expectedSha256, repositoryId, appId, version] {
        InstallOutcome outcome;

        ipkg::PackageVerifier verifier;
        auto verified = verifier.verify(packagePath, expectedSha256);
        if (verified.isError()) {
            outcome.code = verified.error().code;
            outcome.message = verified.error().message;
            return outcome;
        }

        const ipkg::Manifest& manifest = verified.value().manifest;
        if (!appId.isEmpty() && manifest.appId != appId) {
            outcome.code = ErrorCode::RepositoryMismatch;
            outcome.message = QStringLiteral("저장소 정보와 패키지 앱 ID가 다릅니다 (%1 != %2)")
                                  .arg(appId, manifest.appId);
            return outcome;
        }
        if (!version.isEmpty() && manifest.version != version) {
            outcome.code = ErrorCode::RepositoryMismatch;
            outcome.message = QStringLiteral("저장소 정보와 패키지 버전이 다릅니다 (%1 != %2)")
                                  .arg(version, manifest.version);
            return outcome;
        }

        ipkg::Installer installer(paths);
        ipkg::InstallRequest request;
        request.packagePath = packagePath;
        request.expectedSha256 = expectedSha256;
        request.repositoryId = repositoryId;

        auto installed = installer.install(request);
        if (installed.isError()) {
            outcome.code = installed.error().code;
            outcome.message = installed.error().message.isEmpty()
                ? QStringLiteral("설치 실패")
                : installed.error().message;
            return outcome;
        }

        outcome.ok = true;
        outcome.package = installed.value();
        return outcome;
    }));
}

void InstallService::finishSuccess(const InstallOutcome& outcome)
{
    const bool downloaded = m_downloadPath.endsWith(QStringLiteral(".part"));
    if (downloaded)
        QFile::remove(m_downloadPath);

    m_busy = false;
    qInfo("install complete %s %s", qPrintable(outcome.package.appId), qPrintable(outcome.package.version));
    emit installed(outcome.package.appId, outcome.package.version, outcome.package.directory);

    m_currentAppId.clear();
    m_currentVersion.clear();
    m_downloadPath.clear();
}

void InstallService::finishFailure(const QString& appId, const QString& message)
{
    m_busy = false;
    qWarning("install failed %s: %s", qPrintable(appId), qPrintable(message));
    emit failed(appId, message);

    m_currentAppId.clear();
    m_currentVersion.clear();
    m_downloadPath.clear();
}

} // namespace napp::core
