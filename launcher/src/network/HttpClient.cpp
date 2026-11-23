#include "HttpClient.h"

#include "napp/core/json.h"
#include "napp/core/logging.h"

#include <QFile>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace napp::store {
namespace {

constexpr auto kUserAgent = "NAPP/0.1 (+https://illiertion.com)";

QNetworkRequest buildRequest(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setTransferTimeout(30000);
    return request;
}

Error networkError(const QString& message, const QString& detail = {})
{
    return Error::make(ErrorCode::NetworkError, message, detail);
}

} // namespace

HttpClient::HttpClient(QObject* parent)
    : QObject(parent)
{
}

QStringList HttpClient::defaultAllowedHosts()
{
    return {
        QStringLiteral("raw.githubusercontent.com"),
        QStringLiteral("github.com"),
        QStringLiteral("objects.githubusercontent.com"),
        QStringLiteral("codeload.github.com"),
        QStringLiteral("gist.githubusercontent.com"),
    };
}

bool HttpClient::isHostAllowed(const QUrl& url, const QStringList& allowedHosts)
{
    if (url.scheme() != QStringLiteral("https"))
        return false;
    const QString host = url.host().toLower();
    if (host.isEmpty())
        return false;
    const QStringList hosts = allowedHosts.isEmpty() ? defaultAllowedHosts() : allowedHosts;
    for (const QString& allowed : hosts) {
        if (allowed.compare(host, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

void HttpClient::getJson(const QUrl& url, const HttpOptions& options, JsonCallback callback)
{
    fetchRaw(url, options, options.maxRedirects,
             [callback](Result<QByteArray> body, const QUrl& finalUrl) {
                 if (body.isError()) {
                     callback(body.error());
                     return;
                 }
                 auto object = json::parseObject(body.value(), ErrorCode::InvalidDocument,
                                                 finalUrl.toString());
                 if (object.isError()) {
                     callback(object.error());
                     return;
                 }
                 callback(object.value());
             });
}

void HttpClient::fetchRaw(const QUrl& url, const HttpOptions& options, int redirectsLeft,
                          FetchCallback callback)
{
    if (!isHostAllowed(url, options.allowedHosts)) {
        callback(Result<QByteArray>::fail(ErrorCode::HostNotAllowed,
                                          QStringLiteral("허용되지 않은 호스트입니다: %1").arg(url.toString())),
                 url);
        return;
    }

    QNetworkReply* reply = m_manager.get(buildRequest(url));
    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start(options.timeoutMs);

    auto body = std::make_shared<QByteArray>();
    auto overflow = std::make_shared<bool>(false);

    connect(reply, &QNetworkReply::readyRead, reply, [reply, body, overflow, options] {
        body->append(reply->readAll());
        if (body->size() > options.maxBytes) {
            *overflow = true;
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, timer, body, overflow, options, redirectsLeft, callback, url] {
                timer->stop();

                if (*overflow) {
                    reply->deleteLater();
                    callback(Result<QByteArray>::fail(ErrorCode::ResponseTooLarge,
                                                      QStringLiteral("응답 크기 초과: %1").arg(url.toString())),
                             url);
                    return;
                }

                body->append(reply->readAll());
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

                if (status >= 300 && status < 400) {
                    const QUrl location =
                        url.resolved(QUrl::fromEncoded(reply->rawHeader("Location")));
                    reply->deleteLater();
                    if (redirectsLeft <= 0 || !location.isValid()) {
                        callback(Result<QByteArray>::fail(ErrorCode::RedirectLimit,
                                                          QStringLiteral("리다이렉트 한도를 초과했습니다: %1")
                                                              .arg(url.toString())),
                                 location);
                        return;
                    }
                    if (!isHostAllowed(location, options.allowedHosts)) {
                        callback(Result<QByteArray>::fail(ErrorCode::HostNotAllowed,
                                                          QStringLiteral("리다이렉트 대상 호스트가 허용되지 않습니다: %1")
                                                              .arg(location.toString())),
                                 location);
                        return;
                    }
                    fetchRaw(location, options, redirectsLeft - 1, callback);
                    return;
                }

                const QNetworkReply::NetworkError error = reply->error();
                if (error != QNetworkReply::NoError) {
                    const QString message = status > 0
                        ? QStringLiteral("HTTP %1: %2").arg(status).arg(reply->errorString())
                        : reply->errorString();
                    reply->deleteLater();
                    callback(Result<QByteArray>::fail(
                                 status >= 400 ? ErrorCode::HttpStatus : ErrorCode::NetworkError,
                                 QStringLiteral("%1 (%2)").arg(message, url.toString())),
                             url);
                    return;
                }

                if (status != 200 && status != 0) {
                    reply->deleteLater();
                    callback(Result<QByteArray>::fail(ErrorCode::HttpStatus,
                                                      QStringLiteral("예상하지 못한 HTTP 상태 %1 (%2)")
                                                          .arg(status)
                                                          .arg(url.toString())),
                             url);
                    return;
                }

                reply->deleteLater();
                callback(*body, url);
            });
}

void HttpClient::download(const QUrl& url, const QString& destination, const HttpOptions& options,
                          ProgressCallback progress, DownloadCallback callback)
{
    auto follow = std::make_shared<std::function<void(const QUrl&, int)>>();

    *follow = [this, destination, options, progress, callback, follow](const QUrl& target, int redirectsLeft) {
        if (!isHostAllowed(target, options.allowedHosts)) {
            callback(Result<void>::fail(ErrorCode::HostNotAllowed,
                                        QStringLiteral("허용되지 않은 다운로드 호스트: %1").arg(target.toString())));
            return;
        }

        QNetworkReply* reply = m_manager.get(buildRequest(target));
        auto file = std::make_shared<QFile>(destination);
        if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            reply->abort();
            reply->deleteLater();
            callback(Result<void>::fail(ErrorCode::DiskError,
                                        QStringLiteral("다운로드 파일을 만들 수 없습니다: %1").arg(destination)));
            return;
        }

        auto* timer = new QTimer(reply);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
        timer->start(options.timeoutMs * 3); // downloads take longer than JSON

        connect(reply, &QNetworkReply::readyRead, reply, [reply, file, progress] {
            if (!file->isOpen())
                return;
            const QByteArray chunk = reply->readAll();
            file->write(chunk);
            if (progress)
                progress(file->size(), reply->header(QNetworkRequest::ContentLengthHeader).toLongLong());
        });

        connect(reply, &QNetworkReply::finished, this,
                [this, reply, file, timer, target, options, callback, follow, redirectsLeft] {
                    timer->stop();
                    if (file->isOpen())
                        file->close();

                    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    if (status >= 300 && status < 400) {
                        const QUrl location = target.resolved(QUrl::fromEncoded(reply->rawHeader("Location")));
                        reply->deleteLater();
                        QFile::remove(file->fileName());
                        if (redirectsLeft <= 0 || !location.isValid()) {
                            callback(Result<void>::fail(ErrorCode::RedirectLimit,
                                                        QStringLiteral("다운로드 리다이렉트 한도 초과")));
                            return;
                        }
                        (*follow)(location, redirectsLeft - 1);
                        return;
                    }

                    if (reply->error() != QNetworkReply::NoError || status >= 400) {
                        const QString message = QStringLiteral("다운로드 실패: %1 (%2)")
                                                    .arg(reply->errorString(), target.toString());
                        reply->deleteLater();
                        QFile::remove(file->fileName());
                        callback(Result<void>::fail(status >= 400 ? ErrorCode::HttpStatus
                                                                  : ErrorCode::NetworkError,
                                                    message));
                        return;
                    }

                    const qint64 size = QFileInfo(file->fileName()).size();
                    reply->deleteLater();
                    if (size <= 0) {
                        QFile::remove(file->fileName());
                        callback(Result<void>::fail(ErrorCode::NetworkError,
                                                    QStringLiteral("빈 응답을 받았습니다: %1").arg(target.toString())));
                        return;
                    }
                    callback(Result<void>::ok());
                });
    };

    (*follow)(url, options.maxRedirects);
}

} // namespace napp::store
