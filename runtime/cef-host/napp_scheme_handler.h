#pragma once

#include "include/cef_resource_handler.h"
#include "include/cef_scheme.h"

#include "napp_environment.h"

#include <QMutex>
#include <QString>
#include <QStringList>

#include <memory>

namespace napp::runtime {

// Folders the user approved in a native folder dialog during this session. The
// page can only write an export into one of them, so a compromised app cannot
// choose an arbitrary destination path by itself.
// 사용자가 이번 세션에서 네이티브 폴더 대화상자로 고른 폴더들. 페이지는 그중 한 곳에만
// 내보낼 수 있으므로, 앱이 임의 경로를 스스로 고를 수 없다.
class ExportTargets
{
public:
    void approve(const QString& folder);
    bool isApproved(const QString& folder) const;

private:
    mutable QMutex m_mutex;
    QStringList m_folders;
};

using ExportTargetsPtr = std::shared_ptr<ExportTargets>;

// Serves napp://<appId>/<path> from the installed package directory.
// Requests are resolved under appRoot with a containment check, so an app can
// never read files outside its own package (spec §12.3).
class NappResourceHandler : public CefResourceHandler
{
public:
    NappResourceHandler(QString appRoot, QString appId, QString defaultEntry, QString downloadsDir,
                        ExportTargetsPtr targets, NappEnvironment environment);

    bool Open(CefRefPtr<CefRequest> request, bool& handleRequest,
              CefRefPtr<CefCallback> callback) override;
    void GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t& responseLength,
                            CefString& redirectUrl) override;
    bool Read(void* dataOut, int bytesToRead, int& bytesRead,
              CefRefPtr<CefResourceReadCallback> callback) override;
    bool Skip(int64_t bytesToSkip, int64_t& bytesSkipped,
              CefRefPtr<CefResourceSkipCallback> callback) override;
    void Cancel() override {}

private:
    // POST napp://<appId>/__host/export/<name> writes the request body into the
    // app's downloads folder. Apps cannot pick arbitrary destinations.
    void handleExport(const CefRefPtr<CefRequest>& request, const QString& requestedName,
                      const QString& requestedFolder);
    // POST __host/pick-folder opens a native folder dialog on the UI thread and
    // remembers the answer. // UI 스레드에서 네이티브 폴더 대화상자를 열고 결과를 기억한다.
    void handlePickFolder();
    // POST napp://<appId>/__host/notify shows an OS notification for apps that hold
    // the "notifications" permission. // "notifications" 권한이 있는 앱의 OS 알림 요청
    void handleNotify(const CefRefPtr<CefRequest>& request);
    void failExport(int status, const QString& message);

    QString m_appRoot;
    QString m_appId;
    QString m_defaultEntry;
    QString m_downloadsDir;
    ExportTargetsPtr m_targets;
    NappEnvironment m_environment;

    QByteArray m_body;
    qsizetype m_offset = 0;
    QString m_mimeType;
    int m_status = 200;

    IMPLEMENT_REFCOUNTING(NappResourceHandler);
};

class NappSchemeHandlerFactory : public CefSchemeHandlerFactory
{
public:
    NappSchemeHandlerFactory(QString appRoot, QString appId, QString defaultEntry,
                             QString downloadsDir, ExportTargetsPtr targets,
                             NappEnvironment environment);

    CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                         const CefString& schemeName,
                                         CefRefPtr<CefRequest> request) override;

private:
    QString m_appRoot;
    QString m_appId;
    QString m_defaultEntry;
    QString m_downloadsDir;
    ExportTargetsPtr m_targets;
    NappEnvironment m_environment;

    IMPLEMENT_REFCOUNTING(NappSchemeHandlerFactory);
};

} // namespace napp::runtime
