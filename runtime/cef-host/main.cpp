#include "host_options.h"
#include "host_window.h"
#include "napp_environment.h"
#include "napp_scheme_handler.h"

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_helpers.h"

#include "napp/core/logging.h"

#include <atomic>
#include <memory>

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>

namespace {

// CefDoMessageLoopWork() must not run re-entrantly: the scheduled hint below
// and the steady timer can both fire during one Qt dispatch.
bool g_pumping = false;

// Target of OS notifications requested by the page (set once the window exists).
// 페이지가 요청한 OS 알림의 대상 (창이 만들어지면 설정)
std::atomic<napp::runtime::HostWindow*> g_window{nullptr};

void pumpOnce()
{
    if (g_pumping)
        return;
    g_pumping = true;
    CefDoMessageLoopWork();
    g_pumping = false;
}

// Registers the napp:// scheme in every process (browser + renderers).
class NappCefApp : public CefApp, public CefBrowserProcessHandler
{
public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

    void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override
    {
        registrar->AddCustomScheme("napp",
                                   CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE
                                       | CEF_SCHEME_OPTION_CORS_ENABLED | CEF_SCHEME_OPTION_FETCH_ENABLED);
    }

    // Chromium UI is switched off process-wide; the host replaces what it needs
    // with native dialogs and OS notifications. The remaining switches remove
    // background work that only slows startup and costs RAM (8 GB machines).
    // Chromium UI 는 프로세스 전체에서 끈다. 필요한 것은 호스트가 네이티브 대화상자와 OS 알림으로
    // 대체한다. 나머지 스위치는 시작만 느리게 하고 RAM 을 쓰는 백그라운드 작업을 없앤다 (8GB 환경).
    void OnBeforeCommandLineProcessing(const CefString& processType,
                                       CefRefPtr<CefCommandLine> commandLine) override
    {
        Q_UNUSED(processType);
        if (!commandLine)
            return;
        commandLine->AppendSwitch("disable-notifications");
        commandLine->AppendSwitch("disable-dev-tools");
        commandLine->AppendSwitch("disable-extensions");
        commandLine->AppendSwitch("disable-component-update");
        commandLine->AppendSwitch("disable-background-networking");
        commandLine->AppendSwitch("disable-sync");
        commandLine->AppendSwitch("disable-default-apps");
        commandLine->AppendSwitch("disable-domain-reliability");
        commandLine->AppendSwitch("disable-breakpad");
        commandLine->AppendSwitch("disable-print-preview");
        commandLine->AppendSwitch("disable-translate");
        commandLine->AppendSwitch("no-first-run");
        commandLine->AppendSwitch("no-default-browser-check");
        commandLine->AppendSwitchWithValue("password-store", "basic");
        commandLine->AppendSwitchWithValue(
            "disable-features",
            "Translate,MediaRouter,OptimizationHints,AutofillServerCommunication,"
            "CalculateNativeWinOcclusion,PrivacySandboxSettings4,HeavyAdIntervention");

    }

    // CEF only asks for work when it believes something is pending, so this is
    // just a latency hint; the steady timer in main() does the actual pumping.
    void OnScheduleMessagePumpWork(int64_t delayMs) override
    {
        const int delay = static_cast<int>(std::clamp<int64_t>(delayMs, 0, 1000));
        QMetaObject::invokeMethod(qApp, [delay] { QTimer::singleShot(delay, qApp, pumpOnce); },
                                  Qt::QueuedConnection);
    }

private:
    IMPLEMENT_REFCOUNTING(NappCefApp);
};

} // namespace

int main(int argc, char** argv)
{
    QElapsedTimer startup;
    startup.start();

    const napp::runtime::HostOptions options = napp::runtime::parseOptions(argc, argv);

    CefMainArgs mainArgs(GetModuleHandle(nullptr));
    CefRefPtr<NappCefApp> cefApp(new NappCefApp());

    // Subprocesses (renderer, GPU, utility) exit here.
    const int subprocessExitCode = CefExecuteProcess(mainArgs, cefApp, nullptr);
    if (subprocessExitCode >= 0)
        return subprocessExitCode;

    // Qt 6 already makes the process Per-Monitor-V2 DPI aware, and CEF follows that.
    // Do NOT also call CefEnableHighDPISupport(): it would claim DPI awareness first
    // and leave Qt with the weaker "system" mode (blurry on a second monitor).
    // Qt 6 가 이미 프로세스를 Per-Monitor-V2 DPI 인식으로 만들고 CEF 도 이를 따른다.
    // CefEnableHighDPISupport() 를 같이 부르면 먼저 DPI 인식을 선점해서 Qt 가 더 약한
    // "시스템" 모드가 되어 보조 모니터에서 흐려진다.
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication application(argc, argv);
    // The window is hidden while CEF closes the browser; quitting is explicit.
    // CEF 가 브라우저를 닫는 동안 창은 숨겨져 있으므로 종료는 명시적으로 한다.
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setApplicationName(QStringLiteral("NAPP Runtime"));
    QApplication::setOrganizationName(QStringLiteral("ILLIERTION"));

    if (!options.dataDir.isEmpty())
        napp::initLogging(options.logFile());

    if (!options.isValid()) {
        qCritical("invalid runtime arguments: %s", qPrintable(options.errorMessage()));
        return 2;
    }

    if (!QDir().mkpath(options.profileDir())) {
        qCritical("cannot create profile dir: %s", qPrintable(options.profileDir()));
        return 2;
    }

    CefSettings settings;
    settings.no_sandbox = 1;       // TODO(phase 7): link cef_sandbox.lib for real isolation
    settings.external_message_pump = 1;
    settings.windowless_rendering_enabled = 0;
    settings.log_severity = LOGSEVERITY_WARNING;
    // App pages are never inspectable: no remote debugging endpoint exists, so
    // DevTools cannot attach to a running app.
    settings.remote_debugging_port = 0;

    const QString executable = QDir(QCoreApplication::applicationDirPath())
                                   .filePath(QStringLiteral("napp-runtime-host.exe"));
    CefString(&settings.browser_subprocess_path) = executable.toStdString();
    const QString dataDir = QFileInfo(options.dataDir).absoluteFilePath();
    CefString(&settings.root_cache_path) = dataDir.toStdString();
    CefString(&settings.cache_path) = options.profileDir().toStdString();
    CefString(&settings.log_file) =
        QDir(dataDir).filePath(QStringLiteral("logs/cef.log")).toStdString();
    QDir().mkpath(QDir(dataDir).filePath(QStringLiteral("logs")));
    CefString(&settings.user_agent) = "NAPP/0.1 NAPPWebRuntime";
    CefString(&settings.locales_dir_path) =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("locales")).toStdString();

    if (!CefInitialize(mainArgs, settings, cefApp.get(), nullptr)) {
        qCritical("CefInitialize failed");
        return 3;
    }
    qInfo("startup: CefInitialize done after %lld ms", static_cast<long long>(startup.elapsed()));

    napp::runtime::NappEnvironment environment;
    environment.appId = options.appId;
    environment.appName = options.appName;
    environment.language = options.language.startsWith(QStringLiteral("ko"))
                               ? QStringLiteral("ko")
                               : QStringLiteral("en");
    environment.theme = options.theme.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0
                            ? QStringLiteral("light")
                            : QStringLiteral("dark");
    environment.accent = options.accent;
    environment.repositoryUrl = options.repositoryUrl;
    environment.permissions = options.permissions;
    // Page-side Notification -> OS notification. Called on CEF's IO thread, so it
    // only queues work for the UI thread.
    // 페이지의 Notification -> OS 알림. CEF IO 스레드에서 호출되므로 UI 스레드로 작업만 넘긴다.
    environment.notify = [](const QString& title, const QString& body) {
        QMetaObject::invokeMethod(
            qApp,
            [title, body] {
                if (napp::runtime::HostWindow* window = g_window.load())
                    window->showNotification(title, body);
            },
            Qt::QueuedConnection);
    };
    if (!options.runtimeConfig.isEmpty()) {
        QFile file(options.runtimeConfig);
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonObject runtime = QJsonDocument::fromJson(file.readAll()).object();
            environment.runtimeVersion = runtime.value(QStringLiteral("version")).toString();
        }
    }

    // Shared with the scheme handler: only folders approved in the native picker
    // can receive an export. // 네이티브 선택기로 승인된 폴더에만 내보낼 수 있다.
    auto exportTargets = std::make_shared<napp::runtime::ExportTargets>();
    const bool registered = CefRegisterSchemeHandlerFactory(
        "napp", options.appId.toStdString(),
        new napp::runtime::NappSchemeHandlerFactory(
            options.appRoot, options.appId, options.entry,
            QDir(options.dataDir).filePath(QStringLiteral("downloads")), exportTargets,
            environment));
    if (!registered)
        qWarning("napp scheme handler factory registration failed");

    int exitCode = 0;
    {
        napp::runtime::HostWindow window(options);
        g_window.store(&window);
        window.show();
        window.start();
        qInfo("startup: window shown after %lld ms", static_cast<long long>(startup.elapsed()));
        // External message pump: without a steady tick the browser's UI thread
        // can sit idle after the first navigation is queued, leaving the page
        // never committed (a blank window).
        QTimer pumpTimer;
        QObject::connect(&pumpTimer, &QTimer::timeout, pumpOnce);
        pumpTimer.start(15);
        exitCode = QApplication::exec();
        g_window.store(nullptr);
    }

    CefShutdown();
    qInfo("runtime host exiting (%d)", exitCode);
    return exitCode;
}
