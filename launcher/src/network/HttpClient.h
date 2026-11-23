#pragma once

#include "napp/core/result.h"

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QUrl>

#include <functional>

namespace napp::store {

struct HttpOptions
{
    int maxRedirects = 5;
    qint64 maxBytes = 8 * 1024 * 1024;
    int timeoutMs = 20000;
    QStringList allowedHosts; // empty = defaultAllowedHosts()
};

// Asynchronous HTTP(S) access. Redirects are followed manually so every hop is
// checked against the allowlist (GitHub Releases redirects to
// objects.githubusercontent.com), and bodies are capped while streaming.
class HttpClient : public QObject
{
    Q_OBJECT

public:
    explicit HttpClient(QObject* parent = nullptr);

    static QStringList defaultAllowedHosts();
    static bool isHostAllowed(const QUrl& url, const QStringList& allowedHosts);

    using JsonCallback = std::function<void(Result<QJsonObject>)>;
    using DownloadCallback = std::function<void(Result<void>)>;
    using ProgressCallback = std::function<void(qint64 received, qint64 total)>;

    void getJson(const QUrl& url, const HttpOptions& options, JsonCallback callback);

    // Writes the body to `destination` (caller supplies a .part path) and
    // removes it again on failure.
    void download(const QUrl& url, const QString& destination, const HttpOptions& options,
                  ProgressCallback progress, DownloadCallback callback);

private:
    using FetchCallback = std::function<void(Result<QByteArray>, const QUrl&)>;
    void fetchRaw(const QUrl& url, const HttpOptions& options, int redirectsLeft, FetchCallback callback);

    QNetworkAccessManager m_manager;
};

} // namespace napp::store
