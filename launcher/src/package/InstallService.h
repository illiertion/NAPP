#pragma once

#include "napp/core/paths.h"
#include "napp/ipkg/installer.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>

namespace napp::store {
class HttpClient;
}

namespace napp::core {

struct InstallOutcome
{
    bool ok = false;
    ipkg::InstalledPackage package;
    ErrorCode code = ErrorCode::None;
    QString message;
};

// Download → SHA-256 → verify → extract → register. Downloading runs on the
// network thread (QNetworkAccessManager), verification and extraction on a
// worker thread so the UI never blocks.
class InstallService : public QObject
{
    Q_OBJECT

public:
    InstallService(Paths paths, store::HttpClient* http, QObject* parent = nullptr);
    ~InstallService() override;

    bool isBusy() const { return m_busy; }

    bool installFromUrl(const QString& appId, const QString& version, const QUrl& packageUrl,
                        const QString& expectedSha256, const QString& repositoryId);
    bool installFromFile(const QString& packagePath, const QString& repositoryId = QString());

signals:
    void started(const QString& appId, const QString& version);
    void progress(const QString& appId, qint64 received, qint64 total);
    void installed(const QString& appId, const QString& version, const QString& directory);
    void failed(const QString& appId, const QString& message);

private:
    void beginInstall(const QString& packagePath, const QString& appId, const QString& version,
                      const QString& expectedSha256, const QString& repositoryId);
    void finishFailure(const QString& appId, const QString& message);
    void finishSuccess(const InstallOutcome& outcome);

    Paths m_paths;
    store::HttpClient* m_http = nullptr;
    bool m_busy = false;
    QString m_currentAppId;
    QString m_currentVersion;
    QString m_downloadPath;
    QFutureWatcher<InstallOutcome> m_watcher;
};

} // namespace napp::core
