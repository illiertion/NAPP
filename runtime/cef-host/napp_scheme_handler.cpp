#include "napp_scheme_handler.h"

#include "napp/core/logging.h"
#include "napp/core/paths.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QMutexLocker>
#include <QSaveFile>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>

#include <mutex>

namespace napp::runtime {
namespace {

constexpr qint64 kMaxAssetBytes = 32ll * 1024 * 1024;
constexpr qint64 kMaxExportBytes = 64ll * 1024 * 1024;
constexpr auto kExportRoute = "__host/export/";
constexpr auto kNotifyRoute = "__host/notify";
constexpr auto kPickFolderRoute = "__host/pick-folder";

// At most 5 notifications per 10 seconds per host process, so a runaway page
// cannot flood the desktop. // 폭주하는 페이지가 데스크톱을 도배하지 못하게 10초에 5개로 제한
bool allowNotification()
{
    static std::mutex mutex;
    static QElapsedTimer window;
    static int count = 0;
    std::lock_guard<std::mutex> lock(mutex);
    if (!window.isValid() || window.elapsed() > 10000) {
        window.restart();
        count = 0;
    }
    return ++count <= 5;
}

const char* const kContentSecurityPolicy =
    "default-src 'self' napp: data: blob:; "
    "script-src 'self' 'unsafe-inline' 'unsafe-eval' napp: blob:; "
    "style-src 'self' 'unsafe-inline' napp: data:; "
    "img-src 'self' napp: data: blob:; "
    "font-src 'self' napp: data:; "
    "connect-src 'self' napp: https:;";

QString mimeTypeFor(const QString& path)
{
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension);
    if (mime.isValid() && mime.name() != QStringLiteral("application/octet-stream"))
        return mime.name();
    if (path.endsWith(QStringLiteral(".js"), Qt::CaseInsensitive))
        return QStringLiteral("text/javascript");
    if (path.endsWith(QStringLiteral(".mjs"), Qt::CaseInsensitive))
        return QStringLiteral("text/javascript");
    if (path.endsWith(QStringLiteral(".wasm"), Qt::CaseInsensitive))
        return QStringLiteral("application/wasm");
    if (path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        return QStringLiteral("application/json");
    return QStringLiteral("text/plain");
}

QByteArray requestBody(const CefRefPtr<CefRequest>& request)
{
    QByteArray body;
    CefRefPtr<CefPostData> postData = request->GetPostData();
    if (!postData)
        return body;

    CefPostData::ElementVector elements;
    postData->GetElements(elements);
    for (const auto& element : elements) {
        if (!element)
            continue;
        // Upload elements are routinely flagged read-only; the bytes are still
        // readable through GetBytes().
        const size_t count = element->GetBytesCount();
        if (count == 0)
            continue;
        QByteArray chunk(static_cast<int>(count), Qt::Uninitialized);
        if (element->GetBytes(count, chunk.data()) == count)
            body.append(chunk);
    }
    return body;
}

// Windows forbids these in file names; everything else (including Hangul) is fine.
QString safeFileName(const QString& requested)
{
    QString name = QFileInfo(requested).fileName().trimmed();
    for (QChar& character : name) {
        const ushort code = character.unicode();
        if (code < 0x20 || QStringLiteral("<>:\"/\\|?*").contains(character))
            character = QLatin1Char('_');
    }
    while (name.startsWith(QLatin1Char('.')))
        name.remove(0, 1);
    return name;
}

} // namespace

void ExportTargets::approve(const QString& folder)
{
    const QString clean = QDir(folder).absolutePath();
    if (clean.isEmpty())
        return;
    QMutexLocker locker(&m_mutex);
    if (!m_folders.contains(clean))
        m_folders.append(clean);
}

bool ExportTargets::isApproved(const QString& folder) const
{
    const QString clean = QDir(folder).absolutePath();
    QMutexLocker locker(&m_mutex);
    return m_folders.contains(clean);
}

NappResourceHandler::NappResourceHandler(QString appRoot, QString appId, QString defaultEntry,
                                         QString downloadsDir, ExportTargetsPtr targets,
                                         NappEnvironment environment)
    : m_appRoot(std::move(appRoot))
    , m_appId(std::move(appId))
    , m_defaultEntry(std::move(defaultEntry))
    , m_downloadsDir(std::move(downloadsDir))
    , m_targets(std::move(targets))
    , m_environment(std::move(environment))
{
    if (m_environment.appId.isEmpty())
        m_environment.appId = m_appId;
}

bool NappResourceHandler::Open(CefRefPtr<CefRequest> request, bool& handleRequest,
                               CefRefPtr<CefCallback> callback)
{
    handleRequest = true;

    const QUrl url(QString::fromStdString(request->GetURL().ToString()));
    if (url.scheme() != QStringLiteral("napp")) {
        m_status = 400;
        m_body = "bad scheme";
        m_mimeType = QStringLiteral("text/plain");
        return true;
    }

    // One runtime process per app: the host part must be this app's id.
    if (!url.host().isEmpty() && url.host().compare(m_appId, Qt::CaseInsensitive) != 0) {
        qWarning("scheme request for foreign app id: %s", qPrintable(url.host()));
        m_status = 403;
        m_body = "foreign app id";
        m_mimeType = QStringLiteral("text/plain");
        return true;
    }

    QString relative = url.path(QUrl::FullyDecoded);
    while (relative.startsWith(QLatin1Char('/')))
        relative.remove(0, 1);
    if (relative.isEmpty())
        relative = m_defaultEntry;

    // Host routes never touch the package; they let an app hand a build output
    // to the host instead of relying on the browser download manager.
    if (relative.startsWith(QStringLiteral("__host/"))) {
        if (relative == QStringLiteral("__host/info")) {
            m_status = 200;
            m_mimeType = QStringLiteral("application/json");
            m_body = environmentJson(m_environment);
            return true;
        }
        if (relative == QString::fromLatin1(kNotifyRoute)) {
            m_mimeType = QStringLiteral("application/json");
            if (QString::fromStdString(request->GetMethod().ToString()).compare(
                    QStringLiteral("POST"), Qt::CaseInsensitive) != 0) {
                m_status = 405;
                m_body = "{\"error\":\"notify requires POST\"}";
                return true;
            }
            handleNotify(request);
            return true;
        }
        if (relative == QString::fromLatin1(kPickFolderRoute)) {
            m_mimeType = QStringLiteral("application/json");
            if (QString::fromStdString(request->GetMethod().ToString()).compare(
                    QStringLiteral("POST"), Qt::CaseInsensitive)
                != 0) {
                m_status = 405;
                m_body = "{\"error\":\"pick-folder requires POST\"}";
                return true;
            }
            handlePickFolder();
            return true;
        }
        if (!relative.startsWith(QString::fromLatin1(kExportRoute))) {
            m_status = 404;
            m_mimeType = QStringLiteral("text/plain");
            m_body = "unknown host route";
            return true;
        }
        if (QString::fromStdString(request->GetMethod().ToString()).compare(
                QStringLiteral("POST"), Qt::CaseInsensitive)
            != 0) {
            m_status = 405;
            m_mimeType = QStringLiteral("text/plain");
            m_body = "export requires POST";
            return true;
        }
        handleExport(request, relative.mid(QString::fromLatin1(kExportRoute).size()),
                     QUrlQuery(url).queryItemValue(QStringLiteral("folder"), QUrl::FullyDecoded));
        return true;
    }

    const QString absolute = Paths::resolveInside(m_appRoot, relative);
    if (absolute.isEmpty()) {
        qWarning("blocked out-of-root request: %s", qPrintable(relative));
        m_status = 403;
        m_body = "path escapes the package";
        m_mimeType = QStringLiteral("text/plain");
        return true;
    }

    QFileInfo info(absolute);
    if (!info.exists() || !info.isFile()) {
        // SPA style apps: fall back to the entry document for non-asset paths.
        if (!relative.contains(QLatin1Char('.'))) {
            const QString fallback = Paths::resolveInside(m_appRoot, m_defaultEntry);
            if (!fallback.isEmpty() && QFileInfo::exists(fallback)) {
                m_status = 200;
                m_mimeType = QStringLiteral("text/html");
                QFile file(fallback);
                if (!file.open(QIODevice::ReadOnly)) {
                    m_status = 500;
                    m_body = "cannot read entry";
                    return true;
                }
                m_body = file.readAll();
                return true;
            }
        }
        qWarning("missing asset: %s", qPrintable(relative));
        m_status = 404;
        m_body = "not found: " + relative.toUtf8();
        m_mimeType = QStringLiteral("text/plain");
        return true;
    }

    if (info.size() > kMaxAssetBytes) {
        m_status = 413;
        m_body = "asset too large";
        m_mimeType = QStringLiteral("text/plain");
        return true;
    }

    QFile file(absolute);
    if (!file.open(QIODevice::ReadOnly)) {
        m_status = 500;
        m_body = "cannot read asset";
        m_mimeType = QStringLiteral("text/plain");
        return true;
    }

    m_body = file.readAll();
    m_mimeType = mimeTypeFor(absolute);
    // Every HTML document gets window.napp, so installed apps follow the
    // launcher's language, theme and accent without extra setup.
    if (m_mimeType.startsWith(QStringLiteral("text/html"))) {
        m_body = injectEnvironment(m_body, m_environment);
        qInfo("injected environment (lang=%s theme=%s repo=%s)",
              qPrintable(m_environment.language), qPrintable(m_environment.theme),
              qPrintable(m_environment.repositoryUrl));
    }
    m_status = 200;
    qInfo("served %s (%lld bytes, %s)", qPrintable(relative), static_cast<long long>(m_body.size()),
          qPrintable(m_mimeType));
    return true;
}

void NappResourceHandler::handlePickFolder()
{
    // The dialog must run on the Qt UI thread; this handler may be called from
    // CEF's IO thread. A blocking queue keeps the request open until the user
    // answers, which is what the page is waiting for.
    // 대화상자는 Qt UI 스레드에서 열어야 한다. 이 핸들러는 CEF IO 스레드에서 불릴 수 있다.
    // 블로킹 큐로 사용자가 답할 때까지 요청을 열어 둔다.
    QString chosen;
    const auto ask = [this, &chosen] {
        chosen = QFileDialog::getExistingDirectory(
            nullptr, QStringLiteral("NAPP — 저장할 폴더 선택 / Choose folder"),
            QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    };

    if (QThread::currentThread() == qApp->thread())
        ask();
    else
        QMetaObject::invokeMethod(qApp, ask, Qt::BlockingQueuedConnection);

    QJsonObject reply;
    if (chosen.isEmpty()) {
        reply.insert(QStringLiteral("cancelled"), true);
        m_status = 200;
    } else if (m_targets) {
        m_targets->approve(chosen);
        reply.insert(QStringLiteral("path"), QDir::toNativeSeparators(chosen));
        m_status = 200;
    } else {
        failExport(500, QStringLiteral("folder selection is not available"));
        return;
    }
    m_mimeType = QStringLiteral("application/json");
    m_body = QJsonDocument(reply).toJson(QJsonDocument::Compact);
}

void NappResourceHandler::handleExport(const CefRefPtr<CefRequest>& request,
                                       const QString& requestedName,
                                       const QString& requestedFolder)
{
    m_mimeType = QStringLiteral("application/json");

    // The destination is either the app's own downloads folder or a folder the
    // user picked in this session through __host/pick-folder.
    // 저장 위치는 앱의 downloads 폴더이거나, 이번 세션에서 __host/pick-folder 로 고른 폴더다.
    QString destination = m_downloadsDir;
    if (!requestedFolder.isEmpty()) {
        if (!m_targets || !m_targets->isApproved(requestedFolder)) {
            failExport(403, QStringLiteral("the folder was not chosen by the user"));
            return;
        }
        destination = QDir(requestedFolder).absolutePath();
    }
    if (destination.isEmpty()) {
        failExport(500, QStringLiteral("downloads folder is not configured"));
        return;
    }

    const QString name = safeFileName(requestedName);
    if (name.isEmpty()) {
        failExport(400, QStringLiteral("a file name is required"));
        return;
    }

    const QByteArray body = requestBody(request);
    if (body.isEmpty()) {
        failExport(400, QStringLiteral("empty export body"));
        return;
    }
    if (body.size() > kMaxExportBytes) {
        failExport(413, QStringLiteral("export is larger than 64 MB"));
        return;
    }
    if (!QDir().mkpath(destination)) {
        failExport(500, QStringLiteral("cannot create %1").arg(destination));
        return;
    }

    // Never overwrite an earlier export: append " (2)", " (3)", ...
    const QFileInfo info(name);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();
    QString path = QDir(destination).filePath(name);
    for (int index = 2; QFileInfo::exists(path) && index < 1000; ++index) {
        const QString next = suffix.isEmpty()
                                 ? QStringLiteral("%1 (%2)").arg(base).arg(index)
                                 : QStringLiteral("%1 (%2).%3").arg(base).arg(index).arg(suffix);
        path = QDir(destination).filePath(next);
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(body) != body.size() || !file.commit()) {
        failExport(500, QStringLiteral("cannot write %1").arg(path));
        return;
    }

    qInfo("exported %s (%lld bytes)", qPrintable(path), static_cast<long long>(body.size()));

    QJsonObject reply;
    reply.insert(QStringLiteral("path"), path);
    reply.insert(QStringLiteral("name"), QFileInfo(path).fileName());
    reply.insert(QStringLiteral("bytes"), body.size());
    m_status = 201;
    m_body = QJsonDocument(reply).toJson(QJsonDocument::Compact);
}

void NappResourceHandler::handleNotify(const CefRefPtr<CefRequest>& request)
{
    // Only apps whose manifest asked for "notifications" and were granted it.
    // 매니페스트에서 "notifications" 를 요청하고 승인받은 앱만
    if (!m_environment.permissions.contains(QStringLiteral("notifications"))) {
        m_status = 403;
        m_body = "{\"error\":\"notifications permission is not granted\"}";
        return;
    }
    if (!allowNotification()) {
        m_status = 429;
        m_body = "{\"error\":\"too many notifications\"}";
        return;
    }

    const QJsonObject payload = QJsonDocument::fromJson(requestBody(request)).object();
    QString title = payload.value(QStringLiteral("title")).toString().trimmed().left(120);
    const QString body = payload.value(QStringLiteral("body")).toString().trimmed().left(400);
    if (title.isEmpty())
        title = m_environment.appName.isEmpty() ? m_appId : m_environment.appName;

    if (m_environment.notify)
        m_environment.notify(title, body);

    m_status = 202;
    m_body = "{\"shown\":true}";
}

void NappResourceHandler::failExport(int status, const QString& message)
{
    qWarning("export failed (%d): %s", status, qPrintable(message));
    QJsonObject reply;
    reply.insert(QStringLiteral("error"), message);
    m_status = status;
    m_mimeType = QStringLiteral("application/json");
    m_body = QJsonDocument(reply).toJson(QJsonDocument::Compact);
}

void NappResourceHandler::GetResponseHeaders(CefRefPtr<CefResponse> response,
                                             int64_t& responseLength, CefString& redirectUrl)
{
    Q_UNUSED(redirectUrl);

    response->SetStatus(m_status);
    response->SetMimeType(m_mimeType.toStdString());
    response->SetHeaderByName("X-Content-Type-Options", "nosniff", true);
    response->SetHeaderByName("Cross-Origin-Opener-Policy", "same-origin", true);
    response->SetHeaderByName("Cross-Origin-Embedder-Policy", "require-corp", true);
    response->SetHeaderByName("Cache-Control", "no-store", true);
    if (m_mimeType.startsWith(QStringLiteral("text/html")))
        response->SetHeaderByName("Content-Security-Policy", kContentSecurityPolicy, true);

    responseLength = static_cast<int64_t>(m_body.size());
}

bool NappResourceHandler::Read(void* dataOut, int bytesToRead, int& bytesRead,
                               CefRefPtr<CefResourceReadCallback> callback)
{
    Q_UNUSED(callback);

    const qsizetype remaining = m_body.size() - m_offset;
    if (remaining <= 0) {
        bytesRead = 0;
        return false;
    }

    const int chunk = static_cast<int>(qMin<qsizetype>(remaining, bytesToRead));
    memcpy(dataOut, m_body.constData() + m_offset, static_cast<size_t>(chunk));
    m_offset += chunk;
    bytesRead = chunk;
    return true;
}

bool NappResourceHandler::Skip(int64_t bytesToSkip, int64_t& bytesSkipped,
                               CefRefPtr<CefResourceSkipCallback> callback)
{
    Q_UNUSED(callback);

    const qsizetype remaining = m_body.size() - m_offset;
    const qsizetype skipped = qMin<qsizetype>(remaining, static_cast<qsizetype>(bytesToSkip));
    m_offset += skipped;
    bytesSkipped = skipped;
    return true;
}

NappSchemeHandlerFactory::NappSchemeHandlerFactory(QString appRoot, QString appId,
                                                   QString defaultEntry, QString downloadsDir,
                                                   ExportTargetsPtr targets,
                                                   NappEnvironment environment)
    : m_appRoot(std::move(appRoot))
    , m_appId(std::move(appId))
    , m_defaultEntry(std::move(defaultEntry))
    , m_downloadsDir(std::move(downloadsDir))
    , m_targets(std::move(targets))
    , m_environment(std::move(environment))
{
}

CefRefPtr<CefResourceHandler> NappSchemeHandlerFactory::Create(CefRefPtr<CefBrowser> browser,
                                                               CefRefPtr<CefFrame> frame,
                                                               const CefString& schemeName,
                                                               CefRefPtr<CefRequest> request)
{
    Q_UNUSED(browser);
    Q_UNUSED(frame);
    Q_UNUSED(schemeName);
    Q_UNUSED(request);
    return new NappResourceHandler(m_appRoot, m_appId, m_defaultEntry, m_downloadsDir, m_targets,
                                   m_environment);
}

} // namespace napp::runtime
