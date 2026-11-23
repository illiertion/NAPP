#include "MainWindow.h"

#include "LauncherBackend.h"
#include "Theme.h"

#include "core/AppLauncher.h"
#include "napp/core/logging.h"
#include "napp/ipkg/installer.h"
#include "network/HttpClient.h"
#include "package/InstallService.h"
#include "runtime/RuntimeManager.h"
#include "security/PermissionStore.h"
#include "settings/Settings.h"
#include "system/FileAssociation.h"
#include "system/Notifier.h"
#include "napp/core/semver.h"
#include "store/RepositoryService.h"

#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QCursor>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QQuickWidget>
#include <QQmlContext>
#include <QScreen>
#include <QQmlError>
#include <QStyleHints>
#include <QUrl>

#include <algorithm>

namespace napp::ui {

MainWindow::MainWindow(Paths paths, QWidget* parent)
    : QMainWindow(parent)
    , m_paths(std::move(paths))
{
    buildServices();
    buildUi();
    applyTheme();

    // Show the window first, then do the slow work (runtime scan, bundled runtime
    // registration, network refresh). Doing it before show() made the launcher
    // look frozen for several seconds on a cold start.
    // 창을 먼저 띄우고 느린 작업(런타임 스캔, 번들 런타임 등록, 네트워크 갱신)은 나중에 한다.
    // 예전에는 show() 전에 해서 콜드 스타트 때 런처가 몇 초 동안 멈춘 것처럼 보였다.
    QTimer::singleShot(0, this, &MainWindow::deferredStart);
}

void MainWindow::deferredStart()
{
    reconcileRegistry();
    m_runtime->scan();
    prepareBundledRuntime();

    if (m_settings->checkUpdatesOnStartup())
        m_repository->refresh();
    else
        m_backend->setStatus(QStringLiteral("저장소 자동 갱신이 꺼져 있습니다. 앱 스토어에서 새로 고칠 수 있습니다."));

    // Keep the .ipkg association pointing at the running executable (repairs it
    // after the folder was moved or reinstalled).
    // .ipkg 연결이 현재 실행 파일을 가리키도록 유지 (폴더 이동/재설치 후 복구)
    if (m_settings->associateIpkg()) {
        const QString executable = QCoreApplication::applicationFilePath();
        if (!system::FileAssociation::isRegistered(executable))
            system::FileAssociation::registerFor(executable);
    }
}

void MainWindow::prepareBundledRuntime()
{
    // A packaged build ships runtime/<version>/ next to NAPP.exe. Register every
    // bundled runtime that is not installed yet and activate the newest one, so
    // an updated package really replaces an old runtime (before, an old active
    // runtime kept winning and host fixes never took effect).
    // 패키지 빌드는 NAPP.exe 옆에 runtime/<version>/ 을 함께 배포한다. 아직 설치되지 않은
    // 번들 런타임을 모두 등록하고 가장 높은 버전을 활성화한다. (예전에는 기존 런타임이 계속
    // 활성이라 호스트 수정이 반영되지 않았다.)
    const QDir bundledRuntime(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("runtime")));
    const QStringList candidates = bundledRuntime.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    QString newest;
    for (const QString& name : candidates) {
        const QString directory = bundledRuntime.filePath(name);
        if (!QFileInfo::exists(QDir(directory).filePath(QStringLiteral("runtime.json"))))
            continue;
        if (!SemVer::isValid(name))
            continue;

        if (!m_runtime->find(name)) {
            // The copy is large (CEF); tell the user and let the UI paint first.
            // 복사량이 크므로(CEF) 안내를 먼저 보여주고 UI 를 한 번 그린다
            m_backend->setStatus(QStringLiteral("런타임 %1 준비 중... (처음 한 번만 걸립니다)").arg(name));
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            auto registered = m_runtime->registerFromDirectory(directory);
            if (registered.isError()) {
                qWarning("bundled runtime %s: %s", qPrintable(directory),
                         qPrintable(registered.error().message));
                continue;
            }
        }
        const core::InstalledRuntime* installed = nullptr;
        for (const auto& runtime : m_runtime->installed())
            if (runtime.version == name && runtime.valid)
                installed = &runtime;
        if (!installed)
            continue;
        if (newest.isEmpty()
            || SemVer::compareStrings(name, newest).value_or(0) > 0)
            newest = name;
    }

    if (newest.isEmpty())
        return;

    const core::InstalledRuntime* current = m_runtime->active();
    if (!current || SemVer::compareStrings(newest, current->version).value_or(0) > 0) {
        if (m_runtime->activate(newest).isOk())
            qInfo("activated bundled runtime %s", qPrintable(newest));
    }
    m_runtime->scan();
}

MainWindow::~MainWindow()
{
    // Tear the QML scene down while `backend` is still valid; otherwise every
    // binding re-evaluates against a destroyed context property.
    if (m_quick)
        m_quick->setSource(QUrl());
}

QImage MainWindow::snapshot()
{
    if (!m_quick)
        return {};
    return m_quick->grabFramebuffer();
}

void MainWindow::showPage(int index)
{
    if (m_backend)
        m_backend->setCurrentPage(index);
}

void MainWindow::buildServices()
{
    m_installer = std::make_unique<ipkg::Installer>(m_paths);

    m_http = new store::HttpClient(this);
    m_repository = new store::RepositoryService(m_paths, m_http, this);
    m_registry = new core::AppRegistry(m_paths, this);
    m_runtime = new core::RuntimeManager(m_paths, this);

    m_launcher = new core::AppLauncher(m_paths, m_runtime, this);
    m_installService = new core::InstallService(m_paths, m_http, this);
    m_settings = new settings::Settings(m_paths, this);
    m_permissions = new security::PermissionStore(m_paths, this);
    m_notifier = new system::Notifier(this);
    connect(m_notifier, &system::Notifier::activated, this, &MainWindow::raiseAndActivate);

    if (auto loaded = m_settings->load(); loaded.isError())
        qWarning("settings load: %s", qPrintable(loaded.error().message));
    if (auto loaded = m_registry->load(); loaded.isError())
        qWarning("registry load: %s", qPrintable(loaded.error().message));
    if (auto loaded = m_permissions->load(); loaded.isError())
        qWarning("permissions load: %s", qPrintable(loaded.error().message));
    if (auto loaded = m_repository->loadRepositories(); loaded.isError())
        qWarning("repositories load: %s", qPrintable(loaded.error().message));
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("NAPP"));
    setWindowIcon(QIcon(QStringLiteral(":/branding/napp-icon-256.png")));

    // Adapt the shell to the actual desktop and remember the last position.
    // 실제 데스크톱에 맞추고, 마지막 위치/크기를 기억한다.
    setMinimumSize(720, 520);
    restoreOrCenter();

    // Keep the native dialogs in sync when Windows switches light/dark mode
    // while the launcher follows the system theme.
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this] { applyTheme(); });

    m_backend = new LauncherBackend(m_paths, m_repository, m_registry, m_installer.get(),
                                    m_runtime, m_launcher, m_installService, m_settings,
                                    m_permissions, m_notifier, this, this);

    m_quick = new QQuickWidget(this);
    m_quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
    // Dark base colour: no white flash while the scene loads or the window is resized.
    // 어두운 기본색: 장면을 불러오거나 창 크기를 바꿀 때 흰색 깜빡임 방지
    m_quick->setClearColor(m_settings->theme() == settings::ThemeMode::Light ? QColor(0xf4, 0xf6, 0xfa)
                                                                              : QColor(0x08, 0x0a, 0x0e));
    m_quick->rootContext()->setContextProperty(QStringLiteral("backend"), m_backend);
    m_quick->setSource(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    const auto reportErrors = [this](const QString& when) {
        if (m_quick->status() != QQuickWidget::Error)
            return;
        for (const QQmlError& error : m_quick->errors())
            qCritical("qml %s: %s", qPrintable(when), qPrintable(error.toString()));
    };
    reportErrors(QStringLiteral("load"));
    connect(m_quick, &QQuickWidget::statusChanged, this,
            [this, reportErrors](QQuickWidget::Status status) {
                if (status == QQuickWidget::Error)
                    reportErrors(QStringLiteral("runtime"));
            });

    setCentralWidget(m_quick);
}

void MainWindow::applyTheme()
{
    ColorScheme scheme = detectSystemScheme();
    switch (m_settings->theme()) {
    case settings::ThemeMode::Light: scheme = ColorScheme::Light; break;
    case settings::ThemeMode::Dark: scheme = ColorScheme::Dark; break;
    case settings::ThemeMode::System: break;
    }

    const bool pinned = m_settings->theme() != settings::ThemeMode::System;
    if (auto* application = qobject_cast<QApplication*>(QApplication::instance()))
        napp::ui::applyTheme(*application, scheme, m_settings->accent(), pinned);

    if (m_quick)
        m_quick->setPalette(QApplication::palette());
}

void MainWindow::restoreOrCenter()
{
    // Pick the screen under the mouse first run; otherwise the saved geometry decides.
    // 처음엔 마우스가 있는 화면, 이후엔 저장된 위치를 따른다.
    const QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 820);

    bool restored = false;
    const QString saved = m_settings ? m_settings->windowGeometry() : QString();
    if (!saved.isEmpty())
        restored = restoreGeometry(QByteArray::fromBase64(saved.toLatin1()));

    if (restored) {
        // A monitor may have been unplugged or the resolution lowered since the
        // last run: pull the window back onto a visible screen.
        // 모니터를 뺐거나 해상도를 낮췄을 수 있으니 보이는 화면 안으로 되돌린다.
        const QScreen* target = QGuiApplication::screenAt(frameGeometry().center());
        if (!target) {
            restored = false;
        } else {
            available = target->availableGeometry();
            if (width() > available.width() || height() > available.height()) {
                resize(std::min(width(), available.width()), std::min(height(), available.height()));
                move(available.topLeft());
            }
        }
    }

    if (!restored) {
        const int width = std::clamp(int(available.width() * 0.88), 720, 1440);
        const int height = std::clamp(int(available.height() * 0.9), 520, 940);
        // Never exceed the usable area (small laptop screens, 150% scaling).
        // 사용 가능한 영역을 넘지 않게 한다 (작은 노트북 화면, 150% 배율)
        const int fitWidth = std::min(width, available.width());
        const int fitHeight = std::min(height, available.height());
        resize(fitWidth, fitHeight);
        move(available.center() - QPoint(fitWidth / 2, fitHeight / 2));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_settings)
        m_settings->setWindowGeometry(QString::fromLatin1(saveGeometry().toBase64()));
    QMainWindow::closeEvent(event);
}

void MainWindow::raiseAndActivate()
{
    if (isMinimized())
        setWindowState(windowState() & ~Qt::WindowMinimized);
    show();
    raise();
    activateWindow();
}

void MainWindow::openFiles(const QStringList& files)
{
    raiseAndActivate();
    QString first;
    for (const QString& file : files) {
        if (file.endsWith(QStringLiteral(".ipkg"), Qt::CaseInsensitive)) {
            if (first.isEmpty())
                first = file;
            else
                qWarning("only one package is installed at a time, skipping %s", qPrintable(file));
        }
    }
    if (first.isEmpty() || !m_backend)
        return;
    // Let the window come up before the modal approval dialog appears.
    // 모달 승인 대화상자가 뜨기 전에 창이 먼저 나타나게 한다
    QTimer::singleShot(0, this, [this, first] { m_backend->openPackageFile(first); });
}

void MainWindow::reconcileRegistry()
{
    auto ids = m_installer->installedAppIds();
    if (ids.isError())
        return;

    QList<core::RegistryEntry> discovered;
    for (const QString& appId : ids.value()) {
        auto package = m_installer->activePackage(appId);
        if (package.isError())
            continue;
        core::RegistryEntry entry;
        entry.appId = appId;
        entry.name = package.value().name;
        entry.version = package.value().version;
        entry.repositoryId = package.value().repositoryId;
        entry.permissions = package.value().permissions;
        entry.installedAt = QDateTime::currentDateTime();
        discovered.append(entry);
    }
    m_registry->reconcile(discovered);
}

} // namespace napp::ui
