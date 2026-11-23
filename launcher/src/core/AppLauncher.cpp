#include "AppLauncher.h"

#include "napp/core/logging.h"
#include "napp/ipkg/manifest_parser.h"
#include "runtime/RuntimeManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>

namespace napp::core {
namespace {

constexpr auto kRuntimeConfigPrefix = "--runtime-config=";

} // namespace

AppLauncher::AppLauncher(Paths paths, RuntimeManager* runtime, QObject* parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_runtime(runtime)
{
}

void AppLauncher::setEnvironment(const Environment& environment)
{
    m_environment = environment;
}

bool AppLauncher::isRunning(const QString& appId) const
{
    return m_processes.contains(appId);
}

Result<void> AppLauncher::launch(const ipkg::InstalledPackage& package, const QStringList& permissions)
{
    const InstalledRuntime* runtime = m_runtime ? m_runtime->active() : nullptr;
    if (!runtime) {
        return Result<void>::fail(
            ErrorCode::RuntimeMissing,
            QStringLiteral("NAPP Web Runtime이 설치되어 있지 않습니다. 런타임 관리에서 설치하세요."));
    }

    if (isRunning(package.appId)) {
        return Result<void>::fail(ErrorCode::InstallBusy,
                                  QStringLiteral("앱이 이미 실행 중입니다: %1").arg(package.name));
    }

    const QString entryFile = QDir(package.directory).filePath(package.entryRelPath);
    if (!QFileInfo::exists(entryFile)) {
        return Result<void>::fail(ErrorCode::EntryFileMissing,
                                  QStringLiteral("앱 시작 페이지가 없습니다: %1").arg(entryFile));
    }

    const QString dataDirectory = m_paths.dataDir(package.appId);
    if (!QDir().mkpath(dataDirectory)) {
        return Result<void>::fail(ErrorCode::DiskError,
                                  QStringLiteral("앱 데이터 디렉터리를 만들 수 없습니다: %1").arg(dataDirectory));
    }

    auto* process = new QProcess(this);
    process->setProgram(runtime->hostExecutable);
    process->setWorkingDirectory(package.directory);

    QStringList arguments{
        QStringLiteral("--app-id"),
        package.appId,
        QStringLiteral("--app-name"),
        package.name,
        QStringLiteral("--app-root"),
        package.directory,
        QStringLiteral("--entry"),
        package.entryRelPath,
        QStringLiteral("--data-dir"),
        dataDirectory,
        QStringLiteral("--permissions"),
        permissions.join(QLatin1Char(',')),
        QStringLiteral("--language"),
        m_environment.language,
        QStringLiteral("--theme"),
        m_environment.dark ? QStringLiteral("dark") : QStringLiteral("light"),
        QStringLiteral("--accent"),
        m_environment.accent,
        QStringLiteral("--repository-url"),
        m_environment.repositoryUrl,
        QString::fromLatin1(kRuntimeConfigPrefix) + QDir(runtime->directory).filePath(QStringLiteral("runtime.json")),
    };

    // Window preferences only reach the host when the user granted "window";
    // otherwise the app opens with the runtime defaults.
    // 창 설정은 사용자가 "window" 권한을 허용했을 때만 호스트로 전달된다.
    if (permissions.contains(QStringLiteral("window"))) {
        QStringList windowArguments{
            QStringLiteral("--window-title"),
            package.windowTitle,
            QStringLiteral("--window-width"),
            QString::number(package.windowWidth),
            QStringLiteral("--window-height"),
            QString::number(package.windowHeight),
            QStringLiteral("--tray"),
            package.windowTray ? QStringLiteral("1") : QStringLiteral("0"),
        };
        arguments.append(windowArguments);
    }
    process->setArguments(arguments);

    connect(process, &QProcess::errorOccurred, this, [this, process, appId = package.appId](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            const QString reason = process->errorString();
            // `finished` is never emitted when the process did not start, so clean up here.
            // 프로세스가 시작되지 않으면 finished 가 오지 않으므로 여기서 정리
            onProcessFinished(appId);
            emit launchFailed(appId, QStringLiteral("런타임 호스트를 실행할 수 없습니다: %1").arg(reason));
        }
    });

    connect(process, &QProcess::finished, this, [this, appId = package.appId](int exitCode, QProcess::ExitStatus) {
        onProcessFinished(appId);
        emit processExited(appId, exitCode);
    });

    m_processes.insert(package.appId, process);

    // Do not wait for the host here: waiting froze the launcher window for up to
    // 5 seconds on every launch. A start failure arrives via errorOccurred.
    // 여기서 호스트를 기다리지 않는다: 예전에는 실행할 때마다 런처 창이 최대 5초 멈췄다.
    // 시작 실패는 errorOccurred 로 전달된다.
    process->start();

    qInfo("launched %s via %s", qPrintable(package.appId), qPrintable(runtime->hostExecutable));
    return Result<void>::ok();
}

void AppLauncher::onProcessFinished(const QString& appId)
{
    QProcess* process = m_processes.take(appId);
    if (process)
        process->deleteLater();
}

void AppLauncher::terminate(const QString& appId)
{
    QProcess* process = m_processes.value(appId);
    if (!process)
        return;

    // Ask politely first (the host closes its browser cleanly), then force-kill
    // after 3 seconds without blocking the UI thread.
    // 먼저 정상 종료를 요청하고(호스트가 브라우저를 정리), 3초 뒤에도 살아 있으면 UI 를
    // 막지 않고 강제 종료한다.
    process->terminate();
    QPointer<QProcess> guard(process);
    QTimer::singleShot(3000, this, [guard] {
        if (guard && guard->state() != QProcess::NotRunning)
            guard->kill();
    });
}

} // namespace napp::core
