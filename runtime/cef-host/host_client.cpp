#include "host_client.h"

#include "napp/core/logging.h"

#include <QApplication>

#include "include/cef_parser.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QUrl>

namespace napp::runtime {
namespace {

// Windows virtual-key codes used by the shortcut filter. // 단축키 필터가 쓰는 가상 키 코드
constexpr int kKeyF12 = 0x7B;
constexpr int kKeyI = 0x49;
constexpr int kKeyJ = 0x4A;
constexpr int kKeyC = 0x43;
constexpr int kKeyU = 0x55;
constexpr int kKeyP = 0x50;
constexpr int kKeyS = 0x53;

bool isAppScheme(const QString& url)
{
    return url.startsWith(QStringLiteral("napp://"), Qt::CaseInsensitive)
           || url.startsWith(QStringLiteral("blob:napp"), Qt::CaseInsensitive)
           || url.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)
           || url.startsWith(QStringLiteral("about:blank"), Qt::CaseInsensitive);
}

} // namespace

bool NappClient::hasPermission(const char* name) const
{
    return permissions.contains(QString::fromLatin1(name));
}

void NappClient::openExternal(const QString& url) const
{
    // Same rule as the "external-links" permission: http(s) only, only if granted.
    // "external-links" 권한과 같은 규칙: http(s) 만, 승인된 경우에만
    const QUrl target(url);
    if (!hasPermission("external-links"))
        return;
    if (target.scheme() != QStringLiteral("https") && target.scheme() != QStringLiteral("http"))
        return;
    QDesktopServices::openUrl(target);
}

bool NappClient::OnBeforePopup(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int,
                               const CefString& targetUrl, const CefString&,
                               CefLifeSpanHandler::WindowOpenDisposition, bool,
                               const CefPopupFeatures&, CefWindowInfo&, CefRefPtr<CefClient>&,
                               CefBrowserSettings&, CefRefPtr<CefDictionaryValue>&, bool*)
{
    // window.open / target=_blank never creates a Chromium window. A link to the
    // web goes to the system browser if the app may open external links.
    // window.open / target=_blank 는 Chromium 창을 만들지 않는다. 웹 링크는 앱이 외부 링크
    // 권한을 가진 경우에만 시스템 브라우저로 연다.
    const QString url = QString::fromStdString(targetUrl.ToString());
    qInfo("popup blocked: %s", qPrintable(url));
    openExternal(url);
    return true; // true = cancel the popup // true = 팝업 취소
}

void NappClient::OnBeforeContextMenu(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
                                     CefRefPtr<CefContextMenuParams>, CefRefPtr<CefMenuModel> model)
{
    model->Clear(); // an empty model means no menu is shown // 비어 있으면 메뉴가 표시되지 않는다
}

bool NappClient::OnPreKeyEvent(CefRefPtr<CefBrowser>, const CefKeyEvent& event, CefEventHandle,
                               bool*)
{
    if (event.type != KEYEVENT_RAWKEYDOWN && event.type != KEYEVENT_KEYDOWN)
        return false;

    const bool ctrl = (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0;
    const bool shift = (event.modifiers & EVENTFLAG_SHIFT_DOWN) != 0;
    const int key = event.windows_key_code;

    if (key == kKeyF12)
        return true;                                   // DevTools
    if (ctrl && shift && (key == kKeyI || key == kKeyJ || key == kKeyC))
        return true;                                   // DevTools / inspect
    if (ctrl && (key == kKeyU || key == kKeyP || key == kKeyS))
        return true;                                   // view-source / print / save page
    return false;
}

bool NappClient::OnJSDialog(CefRefPtr<CefBrowser>, const CefString&, JSDialogType dialogType,
                            const CefString& messageText, const CefString& defaultPromptText,
                            CefRefPtr<CefJSDialogCallback> callback, bool& suppressMessage)
{
    if (!hasPermission("dialog")) {
        // No "dialog" permission: swallow the dialog (this is also how CEF asks
        // to suppress alert spam). // "dialog" 권한이 없으면 대화상자를 삼킨다
        suppressMessage = true;
        qInfo("js dialog suppressed (no dialog permission)");
        return false;
    }

    // With the permission, show a native OS dialog instead of Chromium's own.
    // It is shown after this callback returns so CEF's pump is not re-entered.
    // 권한이 있으면 Chromium 대신 네이티브 OS 대화상자를 띄운다. CEF 펌프가 재진입되지
    // 않도록 이 콜백이 반환된 뒤에 보여준다.
    const QString message = QString::fromStdString(messageText.ToString());
    const QString defaultText = QString::fromStdString(defaultPromptText.ToString());
    const QString title = appName.isEmpty() ? QStringLiteral("NAPP") : appName;

    QTimer::singleShot(0, qApp, [callback, dialogType, message, defaultText, title] {
        if (dialogType == JSDIALOGTYPE_ALERT) {
            QMessageBox box(QMessageBox::Information, title, message, QMessageBox::Ok);
            box.setWindowFlag(Qt::WindowStaysOnTopHint, true);
            box.exec();
            callback->Continue(true, CefString());
        } else if (dialogType == JSDIALOGTYPE_CONFIRM) {
            QMessageBox box(QMessageBox::Question, title, message,
                            QMessageBox::Ok | QMessageBox::Cancel);
            box.setWindowFlag(Qt::WindowStaysOnTopHint, true);
            callback->Continue(box.exec() == QMessageBox::Ok, CefString());
        } else {
            bool accepted = false;
            const QString text = QInputDialog::getText(nullptr, title, message, QLineEdit::Normal,
                                                       defaultText, &accepted);
            callback->Continue(accepted, text.toStdString());
        }
    });
    return true;
}

bool NappClient::OnBeforeUnloadDialog(CefRefPtr<CefBrowser>, const CefString&, bool,
                                      CefRefPtr<CefJSDialogCallback> callback)
{
    // "Leave site?" never blocks closing the app window. // 앱 창 닫기를 막지 않는다
    callback->Continue(true, CefString());
    return true;
}

bool NappClient::OnBeforeDownload(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem>,
                                  const CefString& suggestedName,
                                  CefRefPtr<CefBeforeDownloadCallback> callback)
{
    if (downloadsDir.isEmpty())
        return true; // not calling Continue cancels the download // Continue 를 안 부르면 취소됨

    QDir().mkpath(downloadsDir);
    QString name = QFileInfo(QString::fromStdString(suggestedName.ToString())).fileName();
    if (name.isEmpty())
        name = QStringLiteral("download");

    // Never overwrite an earlier file. // 이전 파일을 덮어쓰지 않는다
    const QFileInfo info(name);
    QString path = QDir(downloadsDir).filePath(name);
    for (int index = 2; QFileInfo::exists(path) && index < 1000; ++index) {
        path = QDir(downloadsDir).filePath(
            info.suffix().isEmpty()
                ? QStringLiteral("%1 (%2)").arg(info.completeBaseName()).arg(index)
                : QStringLiteral("%1 (%2).%3").arg(info.completeBaseName()).arg(index).arg(info.suffix()));
    }

    // show_dialog=false: no Chromium "Save as" dialog. // Chromium 의 "다른 이름으로 저장" 없음
    callback->Continue(path.toStdString(), false);
    return true;
}

void NappClient::OnDownloadUpdated(CefRefPtr<CefBrowser>, CefRefPtr<CefDownloadItem> item,
                                   CefRefPtr<CefDownloadItemCallback>)
{
    if (!item || !item->IsComplete() || !onNotify)
        return;
    const QString path = QString::fromStdString(item->GetFullPath().ToString());
    onNotify(QStringLiteral("Saved"), QDir::toNativeSeparators(path));
}

bool NappClient::OnBeforeBrowse(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefRequest> request, bool, bool)
{
    if (!frame || !frame->IsMain())
        return false;
    const QString url = QString::fromStdString(request->GetURL().ToString());
    if (isAppScheme(url))
        return false;

    // The app window never navigates away to the web. The link opens in the
    // system browser only when the app holds "external-links".
    // 앱 창은 웹으로 이동하지 않는다. "external-links" 권한이 있을 때만 시스템 브라우저로 연다.
    qInfo("navigation blocked: %s", qPrintable(url));
    openExternal(url);
    return true;
}

void NappClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
    m_browser = browser;
    if (onBrowserCreated)
        onBrowserCreated();
}

void NappClient::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
    if (m_browser && m_browser->IsSame(browser))
        m_browser = nullptr;
    if (onBrowserClosed)
        onBrowserClosed();
}

void NappClient::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                             ErrorCode errorCode, const CefString& errorText,
                             const CefString& failedUrl)
{
    Q_UNUSED(frame);

    // ERR_ABORTED shows up for normal navigation cancellation; ignore it.
    if (errorCode == ERR_ABORTED)
        return;

    const QString url = QString::fromStdString(failedUrl.ToString());
    const QString text = QString::fromStdString(errorText.ToString());
    qWarning("load error %d on %s: %s", int(errorCode), qPrintable(url), qPrintable(text));

    if (onLoadError)
        onLoadError(int(errorCode), text, url);
}

void NappClient::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                           int httpStatusCode)
{
    Q_UNUSED(browser);
    if (!frame || !frame->IsMain())
        return;
    const QString url = QString::fromStdString(frame->GetURL().ToString());
    qInfo("page loaded (%d): %s", httpStatusCode, qPrintable(url));
    if (onLoadFinished)
        onLoadFinished(url);
}

void NappClient::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title)
{
    Q_UNUSED(browser);
    if (onTitleChanged)
        onTitleChanged(QString::fromStdString(title.ToString()));
}

bool NappClient::OnRequestMediaAccessPermission(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& requestingOrigin,
    uint32_t requestedPermissions, CefRefPtr<CefMediaAccessCallback> callback)
{
    Q_UNUSED(browser);
    Q_UNUSED(frame);
    qInfo("media permission granted for %s (flags %u)",
          qPrintable(QString::fromStdString(requestingOrigin.ToString())), requestedPermissions);
    // The callback requires an exact match for getUserMedia requests.
    callback->Continue(requestedPermissions);
    return true;
}

bool NappClient::OnShowPermissionPrompt(CefRefPtr<CefBrowser> browser, uint64_t promptId,
                                        const CefString& requestingOrigin,
                                        uint32_t requestedPermissions,
                                        CefRefPtr<CefPermissionPromptCallback> callback)
{
    Q_UNUSED(browser);
    Q_UNUSED(promptId);

    const bool notifications = (requestedPermissions & CEF_PERMISSION_TYPE_NOTIFICATIONS) != 0;
    qInfo("permission %s for %s (flags %u)", notifications ? "denied" : "granted",
          qPrintable(QString::fromStdString(requestingOrigin.ToString())), requestedPermissions);

    // No Chromium prompt is ever shown. Web notifications are refused here on
    // purpose: the page-side shim (window.Notification, see napp_environment.cpp)
    // already routes them to a real OS notification, so Chromium must not draw a
    // second one. Every other permission is approved silently.
    // Chromium 말풍선은 절대 뜨지 않는다. 웹 알림은 일부러 여기서 거부한다: 페이지의 shim 이
    // 이미 진짜 OS 알림으로 보내므로 Chromium 이 또 하나를 그리면 안 된다.
    callback->Continue(notifications ? CEF_PERMISSION_RESULT_DENY : CEF_PERMISSION_RESULT_ACCEPT);
    return true;
}

bool NappClient::OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level,
                                 const CefString& message, const CefString& source, int line)
{
    Q_UNUSED(browser);
    const QString text = QString::fromStdString(message.ToString());
    const QString origin = QString::fromStdString(source.ToString());
    if (level == LOGSEVERITY_ERROR || level == LOGSEVERITY_FATAL)
        qWarning("console: %s (%s:%d)", qPrintable(text), qPrintable(origin), line);
    else
        qInfo("console: %s (%s:%d)", qPrintable(text), qPrintable(origin), line);
    return false; // also let CEF print it in its own log
}

} // namespace napp::runtime
