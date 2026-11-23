#pragma once

#include "include/cef_client.h"
#include "include/cef_context_menu_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_download_handler.h"
#include "include/cef_jsdialog_handler.h"
#include "include/cef_keyboard_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_permission_handler.h"
#include "include/cef_request_handler.h"

#include <QString>
#include <QStringList>

#include <functional>

namespace napp::runtime {

// Bridges CEF callbacks into Qt-friendly callbacks. CEF runs its message pump
// on the Qt main thread (external_message_pump), so handlers execute there.
//
// Policy: Chromium never draws its own UI. Popups, context menus, permission
// bubbles, JS dialogs, download bars, DevTools and print preview are all blocked
// or replaced by native OS dialogs / notifications driven by the host.
// 정책: Chromium 은 자체 UI 를 절대 그리지 않는다. 팝업, 컨텍스트 메뉴, 권한 말풍선,
// JS 대화상자, 다운로드 표시줄, DevTools, 인쇄 미리보기는 모두 차단하거나 호스트가 띄우는
// 네이티브 OS 대화상자/알림으로 대체한다.
class NappClient : public CefClient,
                   public CefLifeSpanHandler,
                   public CefLoadHandler,
                   public CefDisplayHandler,
                   public CefPermissionHandler,
                   public CefContextMenuHandler,
                   public CefKeyboardHandler,
                   public CefJSDialogHandler,
                   public CefDownloadHandler,
                   public CefRequestHandler
{
public:
    std::function<void()> onBrowserCreated;
    std::function<void()> onBrowserClosed;
    std::function<void(const QString&)> onTitleChanged;
    std::function<void(int, const QString&, const QString&)> onLoadError;
    std::function<void(const QString&)> onLoadFinished;
    // Notification shown by the host (OS toast) for downloads etc.
    // 다운로드 등에 대해 호스트가 띄우는 알림 (OS 토스트)
    std::function<void(const QString&, const QString&)> onNotify;

    // Permissions the user approved for this app, and where downloads are saved.
    // 사용자가 이 앱에 승인한 권한, 그리고 다운로드 저장 위치
    QStringList permissions;
    QString downloadsDir;
    QString appName;

    CefRefPtr<CefBrowser> browser() const { return m_browser; }

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefPermissionHandler> GetPermissionHandler() override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
    CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
    CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override { return this; }
    CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }

    // --- permissions: answered by the host, never by a Chromium bubble -------
    bool OnRequestMediaAccessPermission(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                        const CefString& requestingOrigin,
                                        uint32_t requestedPermissions,
                                        CefRefPtr<CefMediaAccessCallback> callback) override;
    bool OnShowPermissionPrompt(CefRefPtr<CefBrowser> browser, uint64_t promptId,
                                const CefString& requestingOrigin,
                                uint32_t requestedPermissions,
                                CefRefPtr<CefPermissionPromptCallback> callback) override;

    // --- popups: always blocked ------------------------------------------------
    bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int popupId,
                       const CefString& targetUrl, const CefString& targetFrameName,
                       CefLifeSpanHandler::WindowOpenDisposition targetDisposition,
                       bool userGesture, const CefPopupFeatures& popupFeatures,
                       CefWindowInfo& windowInfo, CefRefPtr<CefClient>& client,
                       CefBrowserSettings& settings, CefRefPtr<CefDictionaryValue>& extraInfo,
                       bool* noJavascriptAccess) override;

    // --- no right-click menu ---------------------------------------------------
    void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                             CefRefPtr<CefContextMenuParams> params,
                             CefRefPtr<CefMenuModel> model) override;

    // --- no DevTools / view-source / print shortcuts ---------------------------
    bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
                       CefEventHandle osEvent, bool* isKeyboardShortcut) override;

    // --- JS dialogs: native dialog if the app holds "dialog", otherwise dropped --
    bool OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString& originUrl,
                    JSDialogType dialogType, const CefString& messageText,
                    const CefString& defaultPromptText, CefRefPtr<CefJSDialogCallback> callback,
                    bool& suppressMessage) override;
    bool OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser, const CefString& messageText,
                              bool isReload, CefRefPtr<CefJSDialogCallback> callback) override;

    // --- downloads: saved silently to the app's downloads folder ---------------
    bool OnBeforeDownload(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> downloadItem,
                          const CefString& suggestedName,
                          CefRefPtr<CefBeforeDownloadCallback> callback) override;
    void OnDownloadUpdated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> downloadItem,
                           CefRefPtr<CefDownloadItemCallback> callback) override;

    // --- navigation: the main frame never leaves napp:// -----------------------
    bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefRequest> request, bool userGesture,
                        bool isRedirect) override;

    // Hover link bubble at the bottom-left is Chromium UI: swallow it.
    // 왼쪽 아래에 뜨는 링크 미리보기는 Chromium UI 이므로 삼킨다
    void OnStatusMessage(CefRefPtr<CefBrowser> browser, const CefString& value) override {}

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                     ErrorCode errorCode, const CefString& errorText,
                     const CefString& failedUrl) override;
    void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override;
    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
    // Surfaces page-side errors (missing files, CSP blocks) in runtime.log so an
    // app can be debugged without attaching a remote inspector.
    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level,
                          const CefString& message, const CefString& source,
                          int line) override;

private:
    bool hasPermission(const char* name) const;
    void openExternal(const QString& url) const;

    CefRefPtr<CefBrowser> m_browser;

    IMPLEMENT_REFCOUNTING(NappClient);
};

} // namespace napp::runtime
