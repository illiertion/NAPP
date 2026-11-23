#include "napp/core/logging.h"
#include "napp/core/paths.h"
#include "napp/ipkg/installer.h"

#include "core/AppLauncher.h"
#include "network/HttpClient.h"
#include "runtime/RuntimeManager.h"
#include "security/PermissionStore.h"
#include "settings/Settings.h"
#include "store/RepositoryService.h"
#include "system/SingleInstance.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QStyleHints>
#include <QTextStream>
#include <QTimer>

int main(int argc, char* argv[])
{
    // Pin the QML control style: every control in launcher/qml is styled
    // explicitly, so a platform style cannot leave blank boxes behind.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    // Use the real Windows scale factor (125% / 150%) instead of rounding it to a
    // whole number; rounding is what made the layout look too big or too small.
    // Windows 의 실제 배율(125% / 150%)을 그대로 사용한다. 정수로 반올림하면 화면이 너무 크거나 작게 보인다.
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("NAPP"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setOrganizationName(QStringLiteral("ILLIERTION"));
    QApplication::setOrganizationDomain(QStringLiteral("illiertion.com"));

    const napp::Paths paths = napp::Paths::standard();
    if (!paths.ensureLayout()) {
        qCritical("NAPP 데이터 디렉터리를 만들 수 없습니다: %s", qPrintable(paths.dataRoot()));
        return 1;
    }

    napp::initLogging(paths.logFile());
    qInfo("NAPP launcher starting (data=%s)", qPrintable(paths.dataRoot()));

    // Deployment helper: register a prepared runtime directory without the UI.
    // Used by installers and by the build script to attach a fresh runtime.
    const QStringList arguments = QApplication::arguments();
    const int registerIndex = arguments.indexOf(QStringLiteral("--register-runtime"));
    if (registerIndex >= 0) {
        QTextStream stream(stdout);
        if (registerIndex + 1 >= arguments.size()) {
            stream << "usage: NAPP --register-runtime <runtime-dir>\n";
            return 64;
        }
        napp::core::RuntimeManager runtimeManager(paths);
        runtimeManager.scan();
        auto registered = runtimeManager.registerFromDirectory(arguments.at(registerIndex + 1));
        if (registered.isError()) {
            stream << "registration failed: " << registered.error().message << '\n';
            return 1;
        }
        stream << "registered runtime " << registered.value().version << " at "
               << registered.value().directory << '\n';
        return 0;
    }

    // Deployment helper: install a local .ipkg without the UI. The registry is
    // reconciled from disk on the next GUI start, so this is all that is needed.
    const int installIndex = arguments.indexOf(QStringLiteral("--install-package"));
    if (installIndex >= 0) {
        QTextStream stream(stdout);
        if (installIndex + 1 >= arguments.size()) {
            stream << "usage: NAPP --install-package <path.ipkg>\n";
            return 64;
        }
        napp::ipkg::Installer installer(paths);
        napp::ipkg::InstallRequest request;
        request.packagePath = arguments.at(installIndex + 1);
        auto installed = installer.install(request);
        if (installed.isError()) {
            stream << "install failed: " << installed.error().message << '\n';
            return 1;
        }
        stream << "installed " << installed.value().appId << " v"
               << installed.value().version << " at " << installed.value().directory << '\n';
        return 0;
    }

    // Smoke helper: `NAPP --launch-app <appId> [--launch-seconds N]` starts the
    // real runtime host through AppLauncher, then exits. Used to check that an
    // installed package actually launches without the UI.
    //
    // Shortcut helper: `NAPP --run-app <appId>` runs the app until its window is
    // closed; desktop shortcuts point here. It falls back to the launcher window
    // (Library page) when the app still needs the user to approve permissions.
    // 바로가기 도우미: `NAPP --run-app <appId>` 는 창을 닫을 때까지 앱을 실행한다.
    // 권한 승인이 남아 있으면 런처 창(내 앱 화면)을 연다.
    int pendingPage = -1;
    const int launchIndex = arguments.indexOf(QStringLiteral("--launch-app"));
    const int runIndex = arguments.indexOf(QStringLiteral("--run-app"));
    if (launchIndex >= 0 || runIndex >= 0) {
        QTextStream stream(stdout);
        const bool untilExit = runIndex >= 0;
        const int flagIndex = untilExit ? runIndex : launchIndex;
        if (flagIndex + 1 >= arguments.size()) {
            stream << "usage: NAPP --launch-app <appId> [--launch-seconds N] | "
                      "--run-app <appId>\n";
            return 64;
        }
        const QString appId = arguments.at(flagIndex + 1);
        int seconds = 8;
        const int secondsIndex = arguments.indexOf(QStringLiteral("--launch-seconds"));
        if (secondsIndex >= 0 && secondsIndex + 1 < arguments.size())
            seconds = arguments.at(secondsIndex + 1).toInt();

        napp::core::RuntimeManager runtimeManager(paths);
        runtimeManager.scan();
        if (!runtimeManager.active()) {
            stream << "launch failed: no active runtime\n";
            return 1;
        }

        napp::ipkg::Installer installer(paths);
        auto package = installer.activePackage(appId);
        if (package.isError()) {
            stream << "launch failed: " << package.error().message << '\n';
            return 1;
        }

        napp::security::PermissionStore permissions(paths);
        permissions.load();

        QStringList granted = permissions.granted(appId);
        // Same rule as launching from the Library screen: only a first run (no
        // grants at all) needs the approval dialog.
        // 내 앱 화면에서 실행할 때와 같은 규칙: 한 번도 승인하지 않은 첫 실행만 승인 창이 필요하다.
        const bool approvalsMissing = granted.isEmpty() && !package.value().permissions.isEmpty();

        if (untilExit && approvalsMissing) {
            // Never run with implicit permissions: open the launcher so the user
            // approves them in the normal dialog.
            // 권한을 암묵적으로 부여하고 실행하지 않는다. 런처를 열어 승인받는다.
            pendingPage = 2;
        } else {

        // The GUI passes these values through LauncherBackend; the CLI helper
        // must do the same so a smoke launch sees the real settings.
        napp::settings::Settings settings(paths);
        settings.load();
        napp::store::HttpClient http(&application);
        napp::store::RepositoryService repository(paths, &http);
        repository.loadRepositories();

        napp::core::AppLauncher::Environment environment;
        environment.language = settings.language();
        environment.accent = settings.accent();
        environment.dark =
            settings.theme() == napp::settings::ThemeMode::Dark
            || (settings.theme() == napp::settings::ThemeMode::System
                && application.styleHints()->colorScheme() == Qt::ColorScheme::Dark);
        environment.repositoryUrl = repository.primaryBaseUrl();

        napp::core::AppLauncher launcher(paths, &runtimeManager);
        launcher.setEnvironment(environment);
        QObject::connect(&launcher, &napp::core::AppLauncher::launchFailed, &application,
                         [&stream](const QString& failedId, const QString& message) {
                             stream << "launch failed: " << failedId << " — " << message << '\n';
                             stream.flush();
                             QCoreApplication::exit(1);
                         });

        auto launched = launcher.launch(package.value(), granted);
        if (launched.isError()) {
            stream << "launch failed: " << launched.error().message << '\n';
            return 1;
        }
        stream.flush();

        if (untilExit) {
            // The shortcut-owned window decides when the process ends.
            // 바로가기로 연 창이 닫히면 프로세스도 끝난다.
            QObject::connect(&launcher, &napp::core::AppLauncher::processExited, &application,
                             [&stream](const QString& exitedId, int code) {
                                 stream << "app exited: " << exitedId << " (" << code << ")\n";
                                 stream.flush();
                                 QCoreApplication::quit();
                             });
        } else {
            stream << "launched " << appId << " v" << package.value().version << " for "
                   << seconds << "s\n";
            stream.flush();
            QTimer::singleShot(seconds * 1000, &application, [&launcher, appId, &stream] {
                stream << "stopping " << appId << '\n';
                stream.flush();
                launcher.terminate(appId);
                QCoreApplication::quit();
            });
        }
        return QApplication::exec();
        }
    }

    // Files passed on the command line (double-clicked .ipkg). // 명령줄로 받은 파일 (더블클릭한 .ipkg)
    QStringList packageFiles;
    for (int index = 1; index < arguments.size(); ++index) {
        const QString& argument = arguments.at(index);
        if (argument.startsWith(QLatin1Char('-')))
            continue;
        if (argument.endsWith(QStringLiteral(".ipkg"), Qt::CaseInsensitive))
            packageFiles.append(QFileInfo(argument).absoluteFilePath());
    }

    // Single instance: a second launch hands its files to the running window and
    // exits. (The CLI helpers above deliberately run before this guard.)
    // 단일 실행: 두 번째 실행은 파일을 실행 중인 창에 넘기고 종료한다.
    // (위의 CLI 도우미는 이 보호보다 먼저 실행되도록 일부러 앞에 둔 것)
    napp::system::SingleInstance instance(QDir(paths.configRoot()).absolutePath());
    if (instance.sendToRunning(packageFiles)) {
        qInfo("handed over to the running NAPP launcher");
        return 0;
    }
    if (!instance.listen())
        qWarning("single-instance socket could not be opened; running without it");

    napp::ui::MainWindow window(paths);
    QObject::connect(&instance, &napp::system::SingleInstance::requested, &window,
                     &napp::ui::MainWindow::openFiles);
    window.show();
    if (pendingPage >= 0)
        QTimer::singleShot(0, &window, [&window, pendingPage] { window.showPage(pendingPage); });
    if (!packageFiles.isEmpty())
        QTimer::singleShot(0, &window, [&window, packageFiles] { window.openFiles(packageFiles); });

    // Debug helper: `NAPP --ui-snapshot <dir>` writes one PNG per page and quits.
    const int snapshotIndex = arguments.indexOf(QStringLiteral("--ui-snapshot"));
    if (snapshotIndex >= 0 && snapshotIndex + 1 < arguments.size()) {
        const QString outputDirectory = arguments.at(snapshotIndex + 1);
        QDir().mkpath(outputDirectory);
        // Space the captures out: switching pages can trigger a runtime scan,
        // and timers that fire while the event loop is busy would collapse into
        // one frame (several pages would then share a screenshot).
        for (int page = 0; page < 6; ++page) {
            QTimer::singleShot(1600 + page * 900, &window, [&window, outputDirectory, page] {
                window.showPage(page);
                QTimer::singleShot(450, &window, [&window, outputDirectory, page] {
                    window.snapshot().save(QStringLiteral("%1/page-%2.png").arg(outputDirectory).arg(page));
                });
            });
        }
        QTimer::singleShot(1600 + 6 * 900 + 400, &application, &QCoreApplication::quit);
    }

    return QApplication::exec();
}
