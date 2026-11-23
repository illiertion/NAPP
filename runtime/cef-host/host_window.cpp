#include "host_window.h"

#include "include/cef_browser.h"
#include "napp/core/logging.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>

#include <QAction>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace napp::runtime {

HostWindow::HostWindow(HostOptions options, QWidget* parent)
    : QMainWindow(parent)
    , m_options(std::move(options))
{
    setWindowTitle(m_options.windowTitle.isEmpty()
                       ? (m_options.appName.isEmpty() ? m_options.appId : m_options.appName)
                       : m_options.windowTitle);

    // Fit the screen under the mouse instead of assuming a fixed desktop size: an
    // app window opened on a 1366x768 laptop must not start off-screen or be
    // taller than the usable area.
    // 고정 데스크톱 크기를 가정하지 않고 마우스가 있는 화면에 맞춘다. 1366x768 노트북에서도
    // 화면 밖에서 시작하거나 사용 가능한 영역보다 커지면 안 된다.
    const QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 820);
    // A manifest may ask for a size; it is clamped to the usable screen area.
    // 매니페스트가 크기를 지정할 수 있고, 이때도 화면 사용 가능 영역 안으로 제한한다.
    const int requestedWidth = m_options.windowWidth > 0 ? m_options.windowWidth
                                                       : std::clamp(int(available.width() * 0.86), 640, 1440);
    const int requestedHeight = m_options.windowHeight > 0 ? m_options.windowHeight
                                                         : std::clamp(int(available.height() * 0.88), 480, 960);
    const int width = std::min(available.width(), std::max(360, requestedWidth));
    const int height = std::min(available.height(), std::max(300, requestedHeight));
    resize(width, height);
    setMinimumSize(std::min(360, available.width()), std::min(300, available.height()));
    move(available.center() - QPoint(width / 2, height / 2));

    // Same NAPP mark as the launcher/taskbar icon; the runtime stage ships it
    // next to the host executable.
    const QString iconPath = QDir(QCoreApplication::applicationDirPath())
                                 .filePath(QStringLiteral("napp.ico"));
    if (QFileInfo::exists(iconPath))
        setWindowIcon(QIcon(iconPath));

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_container = new QWidget(central);
    m_container->setAttribute(Qt::WA_NativeWindow); // CEF needs a real HWND
    // Dark base so there is no white flash before the first paint.
    // 첫 화면이 그려지기 전 흰색 깜빡임이 없도록 어두운 바탕
    m_container->setStyleSheet(QStringLiteral("background:#16181d;"));
    layout->addWidget(m_container, 1);

    // The overlay is only shown for errors / the first moments. It must not take
    // layout space once hidden (it used to stay in the layout).
    // 오버레이는 오류/초기 순간에만 보인다. 숨겨진 뒤에는 레이아웃 공간을 차지하면 안 된다.
    m_overlay = new QLabel(QStringLiteral("불러오는 중..."), m_container);
    m_overlay->setAlignment(Qt::AlignCenter);
    m_overlay->setStyleSheet(QStringLiteral("color:#7f8999;background:transparent;"));
    m_overlay->setGeometry(0, 0, 10, 10);

    setCentralWidget(central);

    setUpTray();
}

HostWindow::~HostWindow()
{
    if (m_tray)
        m_tray->hide();
}

void HostWindow::setUpTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(windowIcon(), this);
    m_tray->setToolTip(windowTitle());

    // A resident tray icon is a manifest feature, and like every window feature it
    // needs the "window" permission the user approved at install time.
    // 상주 트레이 아이콘은 매니페스트 기능이며, 다른 창 기능과 마찬가지로 설치 때 승인한
    // "window" 권한이 있어야 한다.
    m_trayResident = m_options.tray && m_options.permissions.contains(QStringLiteral("window"));
    if (!m_trayResident)
        return;

    m_trayMenu = new QMenu(this);
    QAction* open = m_trayMenu->addAction(tr("Open"));
    connect(open, &QAction::triggered, this, [this] { restoreWindow(); });
    m_trayMenu->addSeparator();
    QAction* quit = m_trayMenu->addAction(tr("Quit"));
    connect(quit, &QAction::triggered, this, [this] { quitFromTray(); });
    m_tray->setContextMenu(m_trayMenu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                    restoreWindow();
            });

    m_tray->show();
}

void HostWindow::restoreWindow()
{
    showNormal();
    raise();
    activateWindow();
}

void HostWindow::quitFromTray()
{
    m_quitting = true;
    if (m_tray)
        m_tray->hide();
    if (m_closing)
        return;
    m_closing = true;
    hide();
    closeBrowser();
    QTimer::singleShot(4000, this, [this] { forceQuit(); });
}

void HostWindow::showNotification(const QString& title, const QString& body)
{
    if (!m_tray)
        return;
    if (!m_tray->isVisible())
        m_tray->show();
    const QString heading = title.isEmpty() ? windowTitle() : title;
    m_tray->showMessage(heading, body, QSystemTrayIcon::Information, 6000);
}

void HostWindow::start()
{
    m_client = new NappClient();
    m_client->permissions = m_options.permissions;
    m_client->appName = m_options.appName;
    m_client->downloadsDir = QDir(m_options.dataDir).filePath(QStringLiteral("downloads"));
    m_client->onNotify = [this](const QString& title, const QString& body) {
        showNotification(title, body);
    };
    m_client->onBrowserCreated = [this] {
        m_overlay->hide();
        resizeBrowser();
        qInfo("browser ready for %s (%s)", qPrintable(m_options.appId), qPrintable(m_options.startUrl()));
    };
    m_client->onBrowserClosed = [this] {
        m_browserClosed = true;
        QApplication::quit();
    };
    m_client->onTitleChanged = [this](const QString& title) {
        // A title fixed by the manifest wins: the app author chose it, and a page
        // <title> must not replace it. // 매니페스트가 정한 제목이 우선한다.
        if (title.isEmpty() || !m_options.windowTitle.isEmpty())
            return;
        setWindowTitle(QStringLiteral("%1 — NAPP").arg(title));
    };
    m_client->onLoadFinished = [this](const QString&) { m_overlay->hide(); };
    m_client->onLoadError = [this](int code, const QString& text, const QString& url) {
        m_overlay->setText(QStringLiteral("앱을 불러오지 못했습니다.\n\n%1\n%2\n(오류 %3)")
                               .arg(text, url)
                               .arg(code));
        m_overlay->setGeometry(m_container->rect());
        m_overlay->show();
    };

    // CEF works in physical pixels; Qt reports logical ones. Mixing them is what
    // left the page cropped or too small at 125% / 150% display scaling.
    // CEF 는 물리 픽셀, Qt 는 논리 픽셀을 쓴다. 이를 섞으면 125% / 150% 배율에서 페이지가 잘리거나 작아진다.
    const qreal ratio = m_container->devicePixelRatioF();
    const QSize pixels = (QSizeF(m_container->size()) * ratio).toSize();

    CefWindowInfo windowInfo;
    windowInfo.SetAsChild(reinterpret_cast<CefWindowHandle>(m_container->winId()),
                          CefRect(0, 0, std::max(1, pixels.width()), std::max(1, pixels.height())));
    // An embedded child browser must use the Alloy (content layer) style; the
    // default style assumes a top-level Chrome window.
    // 내장 자식 브라우저는 Alloy(콘텐츠 계층) 스타일을 써야 한다. 기본 스타일은 최상위
    // Chrome 창을 전제로 한다.
    windowInfo.runtime_style = CEF_RUNTIME_STYLE_ALLOY;

    CefBrowserSettings browserSettings;
    browserSettings.background_color = CefColorSetARGB(255, 22, 24, 29);

    const bool created = CefBrowserHost::CreateBrowser(
        windowInfo, m_client, m_options.startUrl().toStdString(), browserSettings, nullptr, nullptr);
    if (!created) {
        m_overlay->setText(QStringLiteral("브라우저 인스턴스를 만들 수 없습니다."));
        m_overlay->setGeometry(m_container->rect());
        m_overlay->show();
        qCritical("CreateBrowser failed for %s", qPrintable(m_options.appId));
    }
}

void HostWindow::resizeBrowser()
{
    if (!m_client)
        return;
    CefRefPtr<CefBrowser> browser = m_client->browser();
    if (!browser)
        return;

#if defined(Q_OS_WIN)
    // A child browser window is a real HWND and does NOT follow its parent on its
    // own: WasResized() alone only re-lays out the page inside the old, fixed
    // rectangle. Move the HWND to the container's current size (physical pixels).
    // 자식 브라우저 창은 진짜 HWND 라서 부모를 자동으로 따라가지 않는다. WasResized() 만 부르면
    // 예전의 고정 사각형 안에서 페이지만 다시 배치된다. HWND 를 컨테이너의 현재 크기(물리 픽셀)로 옮긴다.
    if (HWND hwnd = browser->GetHost()->GetWindowHandle()) {
        const qreal ratio = m_container->devicePixelRatioF();
        const QSize pixels = (QSizeF(m_container->size()) * ratio).toSize();
        SetWindowPos(hwnd, nullptr, 0, 0, std::max(1, pixels.width()), std::max(1, pixels.height()),
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    }
#endif
    browser->GetHost()->NotifyMoveOrResizeStarted();
    browser->GetHost()->WasResized();
}

void HostWindow::closeBrowser()
{
    if (!m_client)
        return;
    CefRefPtr<CefBrowser> browser = m_client->browser();
    if (!browser)
        return;
    browser->GetHost()->CloseBrowser(true);
}

void HostWindow::forceQuit()
{
    if (!m_browserClosed) {
        qWarning("browser did not close in time; quitting anyway");
        QApplication::quit();
    }
}

void HostWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_overlay && m_overlay->isVisible())
        m_overlay->setGeometry(m_container->rect());
    resizeBrowser();
}

void HostWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);

    // Moving the window to a monitor with a different scale factor changes the
    // physical size of the container; follow it.
    // 배율이 다른 모니터로 옮기면 컨테이너의 물리 크기가 바뀌므로 따라간다.
    if (!m_screenHooked && windowHandle()) {
        m_screenHooked = true;
        connect(windowHandle(), &QWindow::screenChanged, this,
                [this] { QTimer::singleShot(0, this, [this] { resizeBrowser(); }); });
    }
    resizeBrowser();
}

void HostWindow::closeEvent(QCloseEvent* event)
{
    // With a resident tray icon, closing the window hides it: the app keeps
    // running until Quit is chosen from the tray menu.
    // 상주 트레이가 있으면 창을 닫아도 숨기기만 하고, 트레이 메뉴의 종료로 끝낸다.
    if (m_trayResident && !m_quitting && !m_browserClosed) {
        event->ignore();
        hide();
        return;
    }

    if (m_browserClosed || !m_client || !m_client->browser()) {
        event->accept();
        QApplication::quit();
        return;
    }

    // Do NOT accept yet: letting Qt destroy the window while the browser is still
    // alive crashed or hung CefShutdown(). Hide the window right away so closing
    // feels instant, ask CEF to close, and quit once OnBeforeClose arrives
    // (with a timeout as a safety net).
    // 아직 accept 하지 않는다: 브라우저가 살아 있는데 Qt 가 창을 파괴하면 CefShutdown() 이
    // 크래시하거나 멈춘다. 닫는 느낌이 즉각적이도록 창은 바로 숨기고, CEF 에 닫기를 요청한 뒤
    // OnBeforeClose 가 오면 종료한다 (안전장치로 타임아웃 사용).
    event->ignore();
    if (m_closing)
        return;
    m_closing = true;
    hide();
    if (m_tray)
        m_tray->hide();
    closeBrowser();
    QTimer::singleShot(4000, this, [this] { forceQuit(); });
}

} // namespace napp::runtime
