#include "LauncherBackend.h"

#include "Theme.h"

#include "core/AppLauncher.h"
#include "napp/core/logging.h"
#include "napp/core/platform.h"
#include "napp/core/semver.h"
#include "package/InstallService.h"
#include "runtime/RuntimeManager.h"
#include "security/PermissionStore.h"
#include "system/FileAssociation.h"
#include "system/Notifier.h"
#include "system/Shortcut.h"
#include "napp/ipkg/package_verifier.h"
#include "settings/Settings.h"
#include "store/RepositoryService.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QAbstractButton>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QStyleHints>
#include <QUrl>

#include <algorithm>

namespace napp::ui {
namespace {

// The launcher UI is translated in QML (components/I18n.qml). Native dialogs
// and model labels are built in C++, so they take the same language setting.
QString pick(const QString& language, const QString& english, const QString& korean)
{
    return language.startsWith(QStringLiteral("ko")) ? korean : english;
}

QString permissionDescription(const QString& permission, const QString& language)
{
    if (permission == QStringLiteral("storage"))
        return pick(language, QStringLiteral("App-private data storage"),
                    QStringLiteral("앱 전용 데이터 저장"));
    if (permission == QStringLiteral("window"))
        return pick(language, QStringLiteral("Change window title and size, keep a tray icon"),
                    QStringLiteral("창 제목/크기 변경, 트레이 아이콘 유지"));
    if (permission == QStringLiteral("notifications"))
        return pick(language, QStringLiteral("Show system notifications"),
                    QStringLiteral("시스템 알림 표시"));
    if (permission == QStringLiteral("dialog"))
        return pick(language, QStringLiteral("Show confirm and message dialogs"),
                    QStringLiteral("확인/메시지 창 표시"));
    if (permission == QStringLiteral("filesystem"))
        return pick(language, QStringLiteral("Access files the user picks"),
                    QStringLiteral("사용자가 선택한 파일 접근"));
    if (permission == QStringLiteral("clipboard"))
        return pick(language, QStringLiteral("Read and write the clipboard"),
                    QStringLiteral("클립보드 읽기/쓰기"));
    if (permission == QStringLiteral("external-links"))
        return pick(language, QStringLiteral("Open external links in the system browser"),
                    QStringLiteral("외부 링크 시스템 브라우저로 열기"));
    if (permission == QStringLiteral("system-info"))
        return pick(language, QStringLiteral("Read OS and runtime information"),
                    QStringLiteral("OS/런타임 정보 조회"));
    return permission;
}

QString appCountText(int count, const QString& language)
{
    if (language.startsWith(QStringLiteral("ko")))
        return QStringLiteral("앱 %1개").arg(count);
    return count == 1 ? QStringLiteral("1 app") : QStringLiteral("%1 apps").arg(count);
}

QString repositoryCountText(int count, const QString& language)
{
    if (language.startsWith(QStringLiteral("ko")))
        return QStringLiteral("저장소 %1개").arg(count);
    return count == 1 ? QStringLiteral("1 repository")
                      : QStringLiteral("%1 repositories").arg(count);
}

QString sizeText(qint64 bytes, const QString& language)
{
    if (bytes <= 0)
        return pick(language, QStringLiteral("Unknown"), QStringLiteral("알 수 없음"));
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024);
    return QStringLiteral("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
}

} // namespace

LauncherBackend::LauncherBackend(Paths paths, store::RepositoryService* repository,
                                 core::AppRegistry* registry, ipkg::Installer* installer,
                                 core::RuntimeManager* runtime, core::AppLauncher* launcher,
                                 core::InstallService* installService, settings::Settings* settings,
                                 security::PermissionStore* permissions, system::Notifier* notifier,
                                 QWidget* dialogParent, QObject* parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_repository(repository)
    , m_registry(registry)
    , m_installer(installer)
    , m_runtime(runtime)
    , m_launcher(launcher)
    , m_installService(installService)
    , m_settings(settings)
    , m_permissions(permissions)
    , m_notifier(notifier)
    , m_dialogParent(dialogParent)
    , m_storeModel({QStringLiteral("appId"), QStringLiteral("name"), QStringLiteral("description"),
                    QStringLiteral("author"), QStringLiteral("category"), QStringLiteral("latest"),
                    QStringLiteral("installedVersion"), QStringLiteral("updateAvailable"),
                    QStringLiteral("supported"), QStringLiteral("iconText")}, this)
    , m_installedModel({QStringLiteral("appId"), QStringLiteral("name"), QStringLiteral("version"),
                        QStringLiteral("permissionsLabel"), QStringLiteral("running"),
                        QStringLiteral("repositoryId"), QStringLiteral("installedDate"),
                        QStringLiteral("updateAvailable"), QStringLiteral("permissionList"),
                        QStringLiteral("iconUrl"), QStringLiteral("hasCustomIcon")}, this)
    , m_updatesModel({QStringLiteral("appId"), QStringLiteral("name"), QStringLiteral("fromVersion"),
                      QStringLiteral("toVersion")}, this)
    , m_runtimeModel({QStringLiteral("version"), QStringLiteral("chromiumVersion"),
                      QStringLiteral("active"), QStringLiteral("valid"), QStringLiteral("problem"),
                      QStringLiteral("directory"), QStringLiteral("stateLabel")}, this)
    , m_repositoryModel({QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("baseUrl"),
                         QStringLiteral("repoEnabled"), QStringLiteral("official")}, this)
    , m_licensesModel({QStringLiteral("name"), QStringLiteral("version"),
                       QStringLiteral("license"), QStringLiteral("url"),
                       QStringLiteral("note")}, this)
    , m_libraryFilter(QStringLiteral("all"))
{
    m_storeStatus = pick(language(), QStringLiteral("Loading repositories..."),
                         QStringLiteral("저장소를 불러오는 중..."));

    rebuildStore();
    rebuildInstalled();
    rebuildUpdates();
    rebuildRuntimes();
    rebuildRepositories();

    connect(m_repository, &store::RepositoryService::catalogChanged, this, [this] {
        rebuildStore();
        rebuildUpdates();
        rebuildInstalled();
    });
    connect(m_repository, &store::RepositoryService::refreshStarted, this, [this] {
        m_storeStatus = pick(language(), QStringLiteral("Refreshing repositories..."),
                             QStringLiteral("저장소를 갱신하는 중..."));
        emit storeStatusChanged();
        setStatus(pick(language(), QStringLiteral("Refreshing repositories"),
                       QStringLiteral("저장소 갱신 중")));
    });
    connect(m_repository, &store::RepositoryService::refreshFinished, this, [this] {
        m_storeStatus = QStringLiteral("%1  ·  %2")
                            .arg(appCountText(static_cast<int>(m_repository->catalog().size()),
                                              language()),
                                 repositoryCountText(
                                     static_cast<int>(m_repository->repositories().size()),
                                     language()));
        emit storeStatusChanged();
        setStatus(pick(language(), QStringLiteral("Repositories refreshed (%1 apps)"),
                       QStringLiteral("저장소 갱신 완료 (%1개 앱)"))
                      .arg(m_repository->catalog().size()));
    });
    connect(m_repository, &store::RepositoryService::repositoryFailed, this,
            [this](const QString& repositoryId, const QString& message) {
                m_storeStatus = QStringLiteral("%1: %2").arg(repositoryId, message);
                emit storeStatusChanged();
                qWarning("repository %s: %s", qPrintable(repositoryId), qPrintable(message));
            });
    connect(m_repository, &store::RepositoryService::statusMessage, this, &LauncherBackend::setStatus);

    connect(m_registry, &core::AppRegistry::changed, this, [this] {
        rebuildInstalled();
        rebuildUpdates();
        rebuildStore();
    });

    connect(m_installService, &core::InstallService::started, this,
            [this](const QString& appId, const QString& version) {
                m_busy = true;
                m_progress = 0.0;
                emit busyChanged();
                emit progressChanged();
                setStatus(pick(language(), QStringLiteral("Downloading %1 v%2..."),
                               QStringLiteral("%1 v%2 다운로드 중..."))
                              .arg(appId, version));
            });
    connect(m_installService, &core::InstallService::progress, this,
            [this](const QString& appId, qint64 received, qint64 total) {
                m_progress = total > 0 ? qreal(received) / qreal(total) : 0.0;
                emit progressChanged();
                setStatus(pick(language(), QStringLiteral("%1 download %2 / %3 KB"),
                               QStringLiteral("%1 다운로드 %2 / %3 KB"))
                              .arg(appId)
                              .arg(received / 1024)
                              .arg(total > 0 ? QString::number(total / 1024) : QStringLiteral("?")));
            });
    connect(m_installService, &core::InstallService::installed, this,
            [this](const QString& appId, const QString& version, const QString& directory) {
                m_busy = false;
                m_progress = 1.0;
                emit busyChanged();
                emit progressChanged();

                auto package = m_installer->activePackage(appId);
                core::RegistryEntry entry;
                entry.appId = appId;
                entry.version = version;
                entry.installedAt = QDateTime::currentDateTime();
                if (package) {
                    entry.name = package.value().name;
                    entry.permissions = package.value().permissions;
                    entry.repositoryId = package.value().repositoryId;
                }
                if (const core::RegistryEntry* previous = m_registry->find(appId))
                    if (entry.repositoryId.isEmpty())
                        entry.repositoryId = previous->repositoryId;
                m_registry->upsert(entry);

                // Permissions are never granted silently. A file install was approved
                // before it started; a store install or update asks right now if the
                // package wants something the user has not allowed yet.
                // 권한은 조용히 부여하지 않는다. 파일 설치는 시작 전에 승인했고, 스토어 설치/업데이트는
                // 아직 허용하지 않은 권한이 있으면 지금 묻는다.
                if (m_pendingGrants.contains(appId)) {
                    m_permissions->setGranted(appId, m_pendingGrants.take(appId));
                } else if (!entry.permissions.isEmpty()) {
                    const QStringList granted = m_permissions->granted(appId);
                    bool missing = false;
                    for (const QString& permission : entry.permissions)
                        if (!granted.contains(permission))
                            missing = true;
                    if (missing) {
                        const QString appName = entry.name.isEmpty() ? appId : entry.name;
                        const QStringList wanted = entry.permissions;
                        QTimer::singleShot(0, this, [this, appId, appName, wanted] {
                            const QString lc = language();
                            const bool allowed = askPermissions(
                                pick(lc, QStringLiteral("%1 was installed and asks for permissions."),
                                     QStringLiteral("%1 설치가 끝났고 권한을 요청합니다."))
                                    .arg(appName),
                                wanted, false);
                            if (allowed) {
                                m_permissions->setGranted(appId, wanted);
                                rebuildInstalled();
                            }
                        });
                    }
                }
                toast(pick(language(), QStringLiteral("Installed"), QStringLiteral("설치 완료")),
                      QStringLiteral("%1 v%2").arg(entry.name.isEmpty() ? appId : entry.name, version));

                rebuildInstalled();
                rebuildUpdates();
                rebuildStore();
                setStatus(pick(language(), QStringLiteral("%1 v%2 installed"),
                               QStringLiteral("%1 v%2 설치 완료"))
                              .arg(appId, version));
                qInfo("installed %s v%s at %s", qPrintable(appId), qPrintable(version),
                      qPrintable(directory));
            });
    connect(m_installService, &core::InstallService::failed, this,
            [this](const QString& appId, const QString& message) {
                m_busy = false;
                emit busyChanged();
                setStatus(pick(language(), QStringLiteral("Install failed: %1"),
                               QStringLiteral("설치 실패: %1"))
                              .arg(message));
                toast(pick(language(), QStringLiteral("Install failed"), QStringLiteral("설치 실패")),
                      message, 2);
                reportError(pick(language(), QStringLiteral("Install failed"),
                                 QStringLiteral("설치 실패")),
                            QStringLiteral("%1\n\n%2").arg(appId, message));
            });
    connect(m_launcher, &core::AppLauncher::launchFailed, this,
            [this](const QString& appId, const QString& message) {
                rebuildInstalled();
                reportError(pick(language(), QStringLiteral("Could not launch"),
                                 QStringLiteral("실행 실패")),
                            QStringLiteral("%1\n\n%2").arg(appId, message));
            });
    connect(m_launcher, &core::AppLauncher::processExited, this,
            [this](const QString& appId, int exitCode) {
                rebuildInstalled();
                setStatus(pick(language(), QStringLiteral("%1 exited (code %2)"),
                               QStringLiteral("%1 종료 (코드 %2)"))
                              .arg(appId)
                              .arg(exitCode));
                if (exitCode != 0)
                    toast(pick(language(), QStringLiteral("App stopped unexpectedly"),
                               QStringLiteral("앱이 비정상 종료되었습니다")),
                          QStringLiteral("%1 (code %2)").arg(appId).arg(exitCode), 1);
            });

    // Follow the OS colour scheme when the user picked "system".
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_settings && m_settings->theme() == settings::ThemeMode::System) {
            applyNativeTheme();
            updateLaunchEnvironment();
            emit themeChanged();
        }
    });

    // The runtime is scanned after the window is shown, so refresh the status when
    // the scan finishes. Without this the rail kept saying "Runtime missing".
    // 런타임 스캔은 창이 뜬 뒤에 끝나므로, 끝나면 상태를 갱신한다.
    // 이게 없으면 사이드바가 계속 "런타임 없음"으로 표시된다.
    connect(m_runtime, &core::RuntimeManager::changed, this, [this] {
        refreshRuntimeState();
        buildLicenses();
    });

    refreshRuntimeState();
    // Needs the scanned runtime so the CEF version in the licence list is real.
    buildLicenses();
    updateLaunchEnvironment();
}

void LauncherBackend::setCurrentPage(int page)
{
    if (page == m_currentPage || page < 0 || page > 5)
        return;
    m_currentPage = page;
    if (page == 2)
        rebuildInstalled();
    if (page == 3)
        refreshRuntimeState();
    emit currentPageChanged();
}

bool LauncherBackend::darkMode() const
{
    if (!m_settings)
        return detectSystemScheme() == ColorScheme::Dark;
    switch (m_settings->theme()) {
    case settings::ThemeMode::Light: return false;
    case settings::ThemeMode::Dark: return true;
    case settings::ThemeMode::System: break;
    }
    return detectSystemScheme() == ColorScheme::Dark;
}

QString LauncherBackend::themeMode() const
{
    return m_settings ? settings::themeToString(m_settings->theme()) : QStringLiteral("system");
}

QString LauncherBackend::accent() const
{
    return m_settings ? m_settings->accent() : QStringLiteral("blue");
}

QString LauncherBackend::language() const
{
    return m_settings ? m_settings->language() : QStringLiteral("en");
}

bool LauncherBackend::checkUpdates() const
{
    return m_settings && m_settings->checkUpdatesOnStartup();
}

void LauncherBackend::setCheckUpdates(bool enabled)
{
    if (!m_settings)
        return;
    m_settings->setCheckUpdatesOnStartup(enabled);
    emit settingsChanged();
}

bool LauncherBackend::associateIpkg() const
{
    return m_settings && m_settings->associateIpkg();
}

void LauncherBackend::setAssociateIpkg(bool enabled)
{
    if (!m_settings)
        return;
    const QString executable = QCoreApplication::applicationFilePath();
    if (enabled)
        napp::system::FileAssociation::registerFor(executable);
    else
        napp::system::FileAssociation::unregister(executable);
    m_settings->setAssociateIpkg(enabled);
    emit settingsChanged();
}

void LauncherBackend::setDetailVisible(bool visible)
{
    if (m_detailVisible == visible)
        return;
    m_detailVisible = visible;
    if (!visible) {
        m_detail.clear();
        m_detailVersions.clear();
    }
    emit detailChanged();
}

void LauncherBackend::setStatus(const QString& message)
{
    if (m_statusText == message)
        return;
    m_statusText = message;
    emit statusTextChanged();
}

QString LauncherBackend::appVersion() const
{
    return QCoreApplication::applicationVersion();
}

QString LauncherBackend::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString LauncherBackend::buildCompiler() const
{
#if defined(_MSC_VER)
    return QStringLiteral("MSVC %1").arg(_MSC_VER);
#elif defined(__clang__)
    return QStringLiteral("Clang %1.%2").arg(__clang_major__).arg(__clang_minor__);
#elif defined(__GNUC__)
    return QStringLiteral("GCC %1.%2").arg(__GNUC__).arg(__GNUC_MINOR__);
#else
    return QStringLiteral("Unknown");
#endif
}

QString LauncherBackend::buildDate() const
{
    return QStringLiteral(__DATE__);
}

QString LauncherBackend::cefVersion() const
{
    const core::InstalledRuntime* active = m_runtime ? m_runtime->active() : nullptr;
    if (!active || active->chromiumVersion.isEmpty())
        return QStringLiteral("—");
    return active->chromiumVersion;
}

QString LauncherBackend::platformName() const
{
    return platformDisplayName(currentPlatformId());
}

QStringList LauncherBackend::categories() const
{
    QStringList categories;
    for (const store::CatalogApp& app : m_repository->catalog()) {
        if (!app.category.isEmpty() && !categories.contains(app.category))
            categories.append(app.category);
    }
    categories.sort();
    return categories;
}

void LauncherBackend::refreshStore()
{
    m_repository->refresh();
}

void LauncherBackend::setStoreQuery(const QString& query)
{
    if (m_storeQuery == query)
        return;
    m_storeQuery = query;
    rebuildStore();
}

void LauncherBackend::setStoreCategory(const QString& category)
{
    if (m_storeCategory == category)
        return;
    m_storeCategory = category;
    rebuildStore();
}

void LauncherBackend::rebuildStore()
{
    const QString platform = currentPlatformId();
    QVector<QVariantMap> rows;

    for (const store::CatalogApp& app : m_repository->catalog()) {
        if (!m_storeQuery.isEmpty()) {
            const bool hit = app.name.contains(m_storeQuery, Qt::CaseInsensitive)
                || app.description.contains(m_storeQuery, Qt::CaseInsensitive)
                || app.id.contains(m_storeQuery, Qt::CaseInsensitive);
            if (!hit)
                continue;
        }
        if (!m_storeCategory.isEmpty() && app.category != m_storeCategory)
            continue;

        const core::RegistryEntry* installed = m_registry->find(app.id);
        const QString best = app.bestVersionFor(platform);
        bool updateAvailable = false;
        if (installed && !best.isEmpty()) {
            const auto comparison = SemVer::compareStrings(best, installed->version);
            updateAvailable = comparison && comparison.value() > 0;
        }

        QVariantMap row;
        row.insert(QStringLiteral("appId"), app.id);
        row.insert(QStringLiteral("name"), app.name);
        row.insert(QStringLiteral("description"), app.description.simplified());
        row.insert(QStringLiteral("author"), app.author);
        row.insert(QStringLiteral("category"), app.category);
        row.insert(QStringLiteral("latest"), app.latest);
        row.insert(QStringLiteral("installedVersion"), installed ? installed->version : QString());
        row.insert(QStringLiteral("updateAvailable"), updateAvailable);
        row.insert(QStringLiteral("supported"),
                   app.platforms.isEmpty() || app.platforms.contains(platform));
        row.insert(QStringLiteral("iconText"), app.name.isEmpty() ? QStringLiteral("?")
                                                                  : app.name.left(1).toUpper());
        rows.append(row);
    }

    std::sort(rows.begin(), rows.end(), [](const QVariantMap& left, const QVariantMap& right) {
        return left.value(QStringLiteral("name")).toString().toLower()
            < right.value(QStringLiteral("name")).toString().toLower();
    });

    m_storeModel.reset(rows);
    emit countsChanged();
}

bool LauncherBackend::hasUpdate(const core::RegistryEntry& entry) const
{
    const store::CatalogApp* app = m_repository->findApp(entry.appId);
    if (!app)
        return false;
    const QString best = app->bestVersionFor(currentPlatformId());
    if (best.isEmpty())
        return false;
    const auto comparison = SemVer::compareStrings(best, entry.version);
    return comparison && comparison.value() > 0;
}

void LauncherBackend::rebuildInstalled()
{
    const QString languageCode = language();
    const auto entries = m_registry->entries();
    m_installedTotal = static_cast<int>(entries.size());

    QVector<QVariantMap> rows;
    for (const core::RegistryEntry& entry : entries) {
        const bool running = m_launcher->isRunning(entry.appId);
        const bool updateAvailable = hasUpdate(entry);

        if (m_libraryFilter == QStringLiteral("running") && !running)
            continue;
        if (m_libraryFilter == QStringLiteral("updates") && !updateAvailable)
            continue;
        if (!m_libraryQuery.isEmpty()) {
            const bool hit = entry.name.contains(m_libraryQuery, Qt::CaseInsensitive)
                || entry.appId.contains(m_libraryQuery, Qt::CaseInsensitive);
            if (!hit)
                continue;
        }

        QStringList labels;
        for (const QString& permission : entry.permissions)
            labels.append(permissionDescription(permission, languageCode));

        QVariantMap row;
        row.insert(QStringLiteral("appId"), entry.appId);
        row.insert(QStringLiteral("name"), entry.name.isEmpty() ? entry.appId : entry.name);
        row.insert(QStringLiteral("version"), entry.version);
        row.insert(QStringLiteral("permissionsLabel"),
                   labels.isEmpty()
                       ? pick(languageCode, QStringLiteral("No permissions"),
                              QStringLiteral("권한 없음"))
                       : labels.join(QStringLiteral(" · ")));
        row.insert(QStringLiteral("permissionList"), labels);
        row.insert(QStringLiteral("running"), running);
        row.insert(QStringLiteral("repositoryId"), entry.repositoryId);
        row.insert(QStringLiteral("installedDate"),
                   entry.installedAt.isValid()
                       ? entry.installedAt.toString(QStringLiteral("yyyy-MM-dd"))
                       : QString());
        row.insert(QStringLiteral("updateAvailable"), updateAvailable);
        row.insert(QStringLiteral("iconUrl"), appIconUrl(entry.appId));
        row.insert(QStringLiteral("hasCustomIcon"), QFileInfo::exists(appIconFile(entry.appId)));
        rows.append(row);
    }

    switch (m_librarySort) {
    case 1:
        std::sort(rows.begin(), rows.end(), [](const QVariantMap& left, const QVariantMap& right) {
            return left.value(QStringLiteral("installedDate")).toString()
                > right.value(QStringLiteral("installedDate")).toString();
        });
        break;
    case 2:
        std::sort(rows.begin(), rows.end(), [](const QVariantMap& left, const QVariantMap& right) {
            return left.value(QStringLiteral("running")).toBool()
                > right.value(QStringLiteral("running")).toBool();
        });
        break;
    default:
        std::sort(rows.begin(), rows.end(), [](const QVariantMap& left, const QVariantMap& right) {
            return left.value(QStringLiteral("name")).toString().toLower()
                < right.value(QStringLiteral("name")).toString().toLower();
        });
        break;
    }

    m_installedModel.reset(rows);
    emit countsChanged();
    emit libraryChanged();
}

void LauncherBackend::rebuildUpdates()
{
    const QString platform = currentPlatformId();
    QVector<QVariantMap> rows;

    for (const core::RegistryEntry& entry : m_registry->entries()) {
        const store::CatalogApp* app = m_repository->findApp(entry.appId);
        if (!app)
            continue;
        const QString best = app->bestVersionFor(platform);
        if (best.isEmpty())
            continue;
        const auto comparison = SemVer::compareStrings(best, entry.version);
        if (!comparison || comparison.value() <= 0)
            continue;

        QVariantMap row;
        row.insert(QStringLiteral("appId"), entry.appId);
        row.insert(QStringLiteral("name"), entry.name.isEmpty() ? entry.appId : entry.name);
        row.insert(QStringLiteral("fromVersion"), entry.version);
        row.insert(QStringLiteral("toVersion"), best);
        rows.append(row);
    }

    m_updatesModel.reset(rows);
    emit countsChanged();
}

void LauncherBackend::rebuildRuntimes()
{
    m_runtime->scan();
    const QString activeVersion = m_runtime->activeVersion();
    const QString languageCode = language();

    QVector<QVariantMap> rows;
    for (const core::InstalledRuntime& runtime : m_runtime->installed()) {
        QVariantMap row;
        row.insert(QStringLiteral("version"), runtime.version);
        row.insert(QStringLiteral("chromiumVersion"),
                   runtime.chromiumVersion.isEmpty()
                       ? pick(languageCode, QStringLiteral("Unknown"),
                              QStringLiteral("알 수 없음"))
                       : runtime.chromiumVersion);
        row.insert(QStringLiteral("active"), runtime.valid && runtime.version == activeVersion);
        row.insert(QStringLiteral("valid"), runtime.valid);
        row.insert(QStringLiteral("problem"), runtime.problem);
        row.insert(QStringLiteral("directory"), runtime.directory);
        row.insert(QStringLiteral("stateLabel"),
                   !runtime.valid
                       ? pick(languageCode, QStringLiteral("Unusable"),
                              QStringLiteral("사용 불가"))
                       : (runtime.version == activeVersion
                              ? pick(languageCode, QStringLiteral("Active"),
                                     QStringLiteral("활성"))
                              : pick(languageCode, QStringLiteral("Available"),
                                     QStringLiteral("사용 가능"))));
        rows.append(row);
    }
    m_runtimeModel.reset(rows);
    emit countsChanged();
}

void LauncherBackend::rebuildRepositories()
{
    updateLaunchEnvironment();
    QVector<QVariantMap> rows;
    for (const store::RepositoryConfig& repository : m_repository->repositories()) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), repository.id);
        row.insert(QStringLiteral("name"), repository.name);
        row.insert(QStringLiteral("baseUrl"), repository.baseUrl);
        row.insert(QStringLiteral("repoEnabled"), repository.enabled);
        row.insert(QStringLiteral("official"), repository.official);
        rows.append(row);
    }
    m_repositoryModel.reset(rows);
    emit countsChanged();
}

void LauncherBackend::buildLicenses()
{
    const QString languageCode = language();
    QVector<QVariantMap> rows;
    const auto push = [&rows](const QString& name, const QString& version, const QString& license,
                              const QString& url, const QString& note) {
        QVariantMap row;
        row.insert(QStringLiteral("name"), name);
        row.insert(QStringLiteral("version"), version);
        row.insert(QStringLiteral("license"), license);
        row.insert(QStringLiteral("url"), url);
        row.insert(QStringLiteral("note"), note);
        rows.append(row);
    };

    push(QStringLiteral("NAPP Launcher"), appVersion(),
         pick(languageCode, QStringLiteral("Proprietary — © 2026 ILLIERTION"),
              QStringLiteral("독점 — © 2026 ILLIERTION")),
         QString(),
         pick(languageCode, QStringLiteral("No LICENSE file is published with this repository."),
              QStringLiteral("이 저장소에는 LICENSE 파일이 없습니다.")));
    push(QStringLiteral("Qt"), qtVersion(),
         QStringLiteral("LGPL-3.0 / commercial"),
         QStringLiteral("https://www.qt.io/licensing/open-source-lgpl-obligations"),
         pick(languageCode, QStringLiteral("Qt Core, Gui, Widgets, Network, Concurrent, Quick."),
              QStringLiteral("Qt Core, Gui, Widgets, Network, Concurrent, Quick.")));
    push(QStringLiteral("Chromium Embedded Framework"), cefVersion(),
         QStringLiteral("BSD-3-Clause"),
         QStringLiteral("https://github.com/chromiumembedded/cef/blob/master/LICENSE.txt"),
         pick(languageCode, QStringLiteral("Web runtime host (runtime/cef-host)."),
              QStringLiteral("웹 런타임 호스트(runtime/cef-host).")));
    push(QStringLiteral("Chromium"), cefVersion(), QStringLiteral("BSD-3-Clause + third-party"),
         QStringLiteral("https://www.chromium.org/Home"),
         pick(languageCode, QStringLiteral("Bundled inside the CEF runtime distribution."),
              QStringLiteral("CEF 런타임 배포본에 포함됩니다.")));
    push(QStringLiteral("miniz"), QStringLiteral("vendored"), QStringLiteral("MIT"),
         QStringLiteral("https://github.com/richgel999/miniz/blob/master/LICENSE"),
         pick(languageCode, QStringLiteral("ZIP reading for .ipkg packages."),
              QStringLiteral(".ipkg 패키지 ZIP 읽기.")));

    m_licensesModel.reset(rows);
}

void LauncherBackend::refreshRuntimeState()
{
    rebuildRuntimes();

    const QString languageCode = language();
    const core::InstalledRuntime* active = m_runtime->active();
    if (active) {
        m_runtimeStatus = pick(languageCode,
                               QStringLiteral("Active runtime: v%1 · Chromium %2 · %3"),
                               QStringLiteral("활성 런타임: v%1 · Chromium %2 · %3"))
                              .arg(active->version,
                                   active->chromiumVersion.isEmpty()
                                       ? pick(languageCode, QStringLiteral("Unknown"),
                                              QStringLiteral("알 수 없음"))
                                       : active->chromiumVersion,
                                   platformDisplayName(currentPlatformId()));
    } else {
        m_runtimeStatus = pick(
            languageCode,
            QStringLiteral("No runtime is active. Install one or register a built runtime "
                           "folder.\nCurrent platform: %1"),
            QStringLiteral("활성화된 런타임이 없습니다. 런타임을 설치하거나 빌드된 런타임 폴더를 "
                           "등록하세요.\n현재 플랫폼: %1"))
                              .arg(platformDisplayName(currentPlatformId()));
    }
    emit runtimeStatusChanged();
}

void LauncherBackend::showAppDetails(const QString& appId)
{
    const store::CatalogApp* app = m_repository->findApp(appId);
    if (!app) {
        reportError(pick(language(), QStringLiteral("App details"),
                         QStringLiteral("앱 정보")),
                    pick(language(), QStringLiteral("The repository does not list this app: %1"),
                         QStringLiteral("저장소에서 앱을 찾을 수 없습니다: %1"))
                        .arg(appId));
        return;
    }

    const QString languageCode = language();
    const core::RegistryEntry* installed = m_registry->find(appId);
    const QString platform = currentPlatformId();

    QStringList versions = app->versions.keys();
    std::sort(versions.begin(), versions.end(), [](const QString& left, const QString& right) {
        return SemVer::compareStrings(left, right).value_or(0) > 0;
    });

    m_detailVersions.clear();
    for (const QString& version : versions) {
        const store::AppVersionInfo* info = app->versionInfo(version);
        if (!info)
            continue;
        const QStringList supported = info->platforms.isEmpty() ? app->platforms : info->platforms;
        const bool platformSupported = supported.isEmpty() || supported.contains(platform);

        QString label = version;
        if (version == app->latest)
            label += pick(languageCode, QStringLiteral("  (latest)"),
                          QStringLiteral("  (최신)"));
        if (installed && installed->version == version)
            label += pick(languageCode, QStringLiteral("  (installed)"),
                          QStringLiteral("  (설치됨)"));
        if (info->yanked)
            label += pick(languageCode, QStringLiteral("  [yanked]"),
                          QStringLiteral("  [차단됨]"));

        QVariantMap entry;
        entry.insert(QStringLiteral("version"), version);
        entry.insert(QStringLiteral("label"), label);
        entry.insert(QStringLiteral("changelog"), info->changelog);
        entry.insert(QStringLiteral("sizeText"), sizeText(info->size, languageCode));
        entry.insert(QStringLiteral("sha256"), info->sha256);
        entry.insert(QStringLiteral("date"),
                     info->releaseDate.isValid()
                         ? info->releaseDate.toString(Qt::ISODate)
                         : pick(languageCode, QStringLiteral("Unknown"),
                                QStringLiteral("알 수 없음")));
        entry.insert(QStringLiteral("supported"), platformSupported);
        entry.insert(QStringLiteral("yanked"), info->yanked);
        entry.insert(QStringLiteral("installed"), installed && installed->version == version);
        m_detailVersions.append(entry);
    }

    m_detail.clear();
    m_detail.insert(QStringLiteral("appId"), app->id);
    m_detail.insert(QStringLiteral("name"), app->name);
    m_detail.insert(QStringLiteral("author"), app->author);
    m_detail.insert(QStringLiteral("description"), app->description);
    m_detail.insert(QStringLiteral("category"), app->category);
    m_detail.insert(QStringLiteral("repository"), m_repository->repositoryName(app->repositoryId));
    m_detail.insert(QStringLiteral("homepage"), app->homepage);
    m_detail.insert(QStringLiteral("latest"), app->latest);
    m_detail.insert(QStringLiteral("installedVersion"), installed ? installed->version : QString());
    m_detail.insert(QStringLiteral("versions"), m_detailVersions);
    m_detail.insert(QStringLiteral("iconUrl"), appIconUrl(app->id));

    m_detailVisible = true;
    emit detailChanged();
}

void LauncherBackend::installApp(const QString& appId, const QString& version)
{
    const QString languageCode = language();
    const store::CatalogApp* app = m_repository->findApp(appId);
    if (!app) {
        reportError(pick(languageCode, QStringLiteral("Install failed"),
                         QStringLiteral("설치 실패")),
                    pick(languageCode, QStringLiteral("Unknown app: %1"),
                         QStringLiteral("앱 정보를 찾을 수 없습니다: %1"))
                        .arg(appId));
        return;
    }
    const store::AppVersionInfo* info = app->versionInfo(version);
    if (!info) {
        reportError(pick(languageCode, QStringLiteral("Install failed"),
                         QStringLiteral("설치 실패")),
                    pick(languageCode, QStringLiteral("Unknown version: %1"),
                         QStringLiteral("버전 정보가 없습니다: %1"))
                        .arg(version));
        return;
    }
    if (info->yanked) {
        reportError(pick(languageCode, QStringLiteral("Install failed"),
                         QStringLiteral("설치 실패")),
                    pick(languageCode, QStringLiteral("The publisher yanked this version."),
                         QStringLiteral("배포자가 차단한 버전입니다")));
        return;
    }

    setDetailVisible(false);
    if (!m_installService->installFromUrl(appId, version, info->url, info->sha256,
                                          app->repositoryId))
        reportError(pick(languageCode, QStringLiteral("Install failed"),
                         QStringLiteral("설치 실패")),
                    pick(languageCode, QStringLiteral("Another install is already running."),
                         QStringLiteral("다른 설치 작업이 진행 중입니다")));
}

void LauncherBackend::installFromFile()
{
    const QString path = QFileDialog::getOpenFileName(
        m_dialogParent,
        pick(language(), QStringLiteral("Select an IPKG package"),
             QStringLiteral("IPKG 패키지 선택")),
        QString(), QStringLiteral("NAPP package (*.ipkg)"));
    if (path.isEmpty())
        return;
    openPackageFile(path);
}

void LauncherBackend::openPackageFile(const QString& path)
{
    const QString lc = language();
    const QString failTitle = pick(lc, QStringLiteral("Install failed"), QStringLiteral("설치 실패"));

    if (m_installService->isBusy()) {
        toast(failTitle, pick(lc, QStringLiteral("Another install is already running."),
                              QStringLiteral("다른 설치 작업이 진행 중입니다")), 1);
        return;
    }

    ipkg::PackageVerifier verifier;
    auto info = verifier.verify(path);
    if (info.isError()) {
        toast(failTitle, info.error().message, 2);
        reportError(failTitle, QStringLiteral("%1\n\n%2").arg(path, info.error().message));
        return;
    }
    const ipkg::Manifest& manifest = info.value().manifest;
    if (auto platform = verifier.checkPlatform(manifest); platform.isError()) {
        toast(failTitle, platform.error().message, 2);
        reportError(failTitle, platform.error().message);
        return;
    }

    // One native approval covers "install this" and "grant these permissions".
    // 네이티브 승인 한 번으로 "설치"와 "권한 부여"를 함께 처리
    const core::RegistryEntry* previous = m_registry->find(manifest.appId);
    const QString headline =
        previous ? pick(lc, QStringLiteral("Update %1 from v%2 to v%3?"),
                        QStringLiteral("%1 을(를) v%2 에서 v%3 로 업데이트할까요?"))
                       .arg(manifest.name, previous->version, manifest.version)
                 : pick(lc, QStringLiteral("Install %1 v%2?"),
                        QStringLiteral("%1 v%2 을(를) 설치할까요?"))
                       .arg(manifest.name, manifest.version);
    if (!askPermissions(headline, manifest.permissions, false)) {
        setStatus(pick(lc, QStringLiteral("Install cancelled"), QStringLiteral("설치를 취소했습니다")));
        return;
    }

    m_pendingGrants.insert(manifest.appId, manifest.permissions);
    setCurrentPage(2);
    if (!m_installService->installFromFile(path))
        m_pendingGrants.remove(manifest.appId);
}

void LauncherBackend::launchApp(const QString& appId)
{
    const QString languageCode = language();
    auto package = m_installer->activePackage(appId);
    if (package.isError()) {
        reportError(pick(languageCode, QStringLiteral("Could not launch"),
                         QStringLiteral("실행 실패")),
                    package.error().message);
        return;
    }

    if (!m_runtime->active()) {
        const auto reply = QMessageBox::question(
            m_dialogParent,
            pick(languageCode, QStringLiteral("Runtime required"),
                 QStringLiteral("런타임 필요")),
            pick(languageCode,
                 QStringLiteral("The NAPP Web Runtime is not installed.\nOpen Runtime "
                                "management?"),
                 QStringLiteral("NAPP Web Runtime이 설치되어 있지 않습니다.\n런타임 관리로 "
                                "이동할까요?")),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes)
            setCurrentPage(3);
        return;
    }

    QStringList permissions = m_permissions->granted(appId);
    if (permissions.isEmpty() && !package.value().permissions.isEmpty()) {
        const bool allowed = askPermissions(
            pick(languageCode, QStringLiteral("%1 needs the following permissions to run."),
                 QStringLiteral("%1 실행에 다음 권한이 필요합니다."))
                .arg(package.value().name),
            package.value().permissions, false);
        if (!allowed)
            return;
        m_permissions->setGranted(appId, package.value().permissions);
        permissions = package.value().permissions;
    }

    auto launched = m_launcher->launch(package.value(), permissions);
    if (launched.isError()) {
        reportError(pick(languageCode, QStringLiteral("Could not launch"),
                         QStringLiteral("실행 실패")),
                    launched.error().message);
        return;
    }
    rebuildInstalled();
    setStatus(pick(languageCode, QStringLiteral("%1 is running"),
                   QStringLiteral("%1 실행 중"))
                  .arg(package.value().name));
}

void LauncherBackend::stopApp(const QString& appId)
{
    if (!m_launcher->isRunning(appId))
        return;
    m_launcher->terminate(appId);
    rebuildInstalled();
    setStatus(pick(language(), QStringLiteral("Stopped %1"), QStringLiteral("%1 종료"))
                  .arg(appId));
}

void LauncherBackend::removeApp(const QString& appId)
{
    const QString languageCode = language();
    const auto reply = QMessageBox::question(
        m_dialogParent,
        pick(languageCode, QStringLiteral("Remove app"), QStringLiteral("앱 제거")),
        pick(languageCode,
             QStringLiteral("The program files will be deleted. Delete the app data too?\n\n"
                            "App ID: %1"),
             QStringLiteral("프로그램 파일을 삭제합니다. 앱 데이터도 함께 삭제할까요?\n\n"
                            "앱 ID: %1"))
            .arg(appId),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (reply == QMessageBox::Cancel)
        return;

    auto removed = m_installer->remove(appId, reply == QMessageBox::Yes);
    if (removed.isError()) {
        reportError(pick(languageCode, QStringLiteral("Remove failed"),
                         QStringLiteral("제거 실패")),
                    removed.error().message);
        return;
    }
    m_permissions->clear(appId);
    m_registry->remove(appId);
    // A stale icon must not be picked up by a future app that reuses the id.
    // 아이디를 재사용하는 앱이 예전 아이콘을 물려받지 않도록 지운다.
    {
        const QString pngPath = appIconFile(appId);
        QFile::remove(pngPath);
        QFile::remove(QFileInfo(pngPath).absolutePath() + QLatin1Char('/')
                      + QFileInfo(pngPath).completeBaseName() + QStringLiteral(".ico"));
    }
    rebuildInstalled();
    rebuildUpdates();
    rebuildStore();
    setStatus(pick(languageCode, QStringLiteral("Removed %1"), QStringLiteral("앱을 제거했습니다: %1"))
                  .arg(appId));
}

void LauncherBackend::openDataFolder(const QString& appId)
{
    const QString path = m_paths.dataDir(appId);
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void LauncherBackend::editPermissions(const QString& appId)
{
    const QString languageCode = language();
    auto package = m_installer->activePackage(appId);
    if (package.isError())
        return;

    QStringList lines;
    for (const QString& permission : package.value().permissions)
        lines.append(QStringLiteral("· %1").arg(permissionDescription(permission, languageCode)));
    if (lines.isEmpty())
        lines.append(pick(languageCode, QStringLiteral("This app requests no permissions."),
                          QStringLiteral("이 앱은 권한을 요청하지 않습니다.")));

    const QStringList granted = m_permissions->granted(appId);
    const auto reply = QMessageBox::question(
        m_dialogParent,
        pick(languageCode, QStringLiteral("App permissions"), QStringLiteral("앱 권한")),
        pick(languageCode,
             QStringLiteral("%1 requests:\n\n%2\n\nCurrently allowed: %3\n\nAllow all? "
                            "(No: revoke all)"),
             QStringLiteral("%1 이(가) 요청한 권한:\n\n%2\n\n현재 허용: %3\n\n모두 허용할까요? "
                            "(아니오: 모두 취소)"))
            .arg(package.value().name, lines.join(QLatin1Char('\n')),
                 granted.isEmpty() ? pick(languageCode, QStringLiteral("none"),
                                          QStringLiteral("없음"))
                                   : granted.join(QStringLiteral(", "))),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_permissions->setGranted(appId, package.value().permissions);
        rebuildInstalled();
        setStatus(pick(languageCode, QStringLiteral("Permissions granted: %1"),
                       QStringLiteral("권한을 허용했습니다: %1"))
                      .arg(appId));
    } else {
        m_permissions->setGranted(appId, {});
        rebuildInstalled();
        setStatus(pick(languageCode, QStringLiteral("All permissions revoked: %1"),
                       QStringLiteral("모든 권한을 취소했습니다: %1"))
                      .arg(appId));
    }
}

QString LauncherBackend::appIconDirectory() const
{
    return QDir(m_paths.configRoot()).filePath(QStringLiteral("icons"));
}

QString LauncherBackend::appIconFile(const QString& appId) const
{
    // The app id is the file name, so two apps can never fight over one icon.
    // 앱 아이디를 파일 이름으로 써서 두 앱이 같은 아이콘을 두고 충돌하지 않게 한다.
    QString safe = appId;
    for (QChar& character : safe) {
        const ushort code = character.unicode();
        if (code < 0x20 || QStringLiteral("<>:\"/\\|?*").contains(character))
            character = QLatin1Char('_');
    }
    return QDir(appIconDirectory()).filePath(safe + QStringLiteral(".png"));
}

QString LauncherBackend::appIconUrl(const QString& appId) const
{
    // A user-picked icon wins; otherwise the icon shipped inside the package is
    // drawn, so a package built with its own artwork looks right immediately.
    // 사용자가 고른 아이콘이 우선이고, 없으면 패키지에 들어 있는 아이콘을 그린다.
    QString path = appIconFile(appId);
    QFileInfo info(path);
    if (!info.exists()) {
        auto package = m_installer->activePackage(appId);
        if (package.isError() || package.value().iconRelPath.isEmpty())
            return QString();
        const QString packageIcon = Paths::resolveInside(package.value().directory,
                                                         package.value().iconRelPath);
        if (packageIcon.isEmpty())
            return QString();
        info = QFileInfo(packageIcon);
        if (!info.exists())
            return QString();
        path = packageIcon;
    }
    // The version suffix defeats QQmlEngine's image cache: a newly picked icon
    // has the same path but a newer timestamp.
    // 경로는 같고 수정 시간만 다르므로, 버전 접미사로 이미지 캐시를 무효화한다.
    QUrl url = QUrl::fromLocalFile(path);
    url.setQuery(QStringLiteral("v=%1").arg(info.lastModified().toSecsSinceEpoch()));
    return url.toString();
}

void LauncherBackend::writeAppIcon(const QString& appId, const QImage& image)
{
    QDir().mkpath(appIconDirectory());

    QImage scaled = image;
    if (scaled.width() > 512 || scaled.height() > 512)
        scaled = scaled.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    const QString pngPath = appIconFile(appId);
    if (!scaled.save(pngPath, "PNG")) {
        reportError(pick(language(), QStringLiteral("Icon"), QStringLiteral("아이콘")),
                    pick(language(), QStringLiteral("Could not save the icon to %1"),
                         QStringLiteral("아이콘을 저장할 수 없습니다: %1"))
                        .arg(pngPath));
        return;
    }

    // Shortcuts need an .ico; the PNG above is what the launcher draws.
    // 바로가기에는 .ico 가 필요하고, 위의 PNG 는 런처 화면이 사용한다.
    scaled.save(QFileInfo(pngPath).absolutePath() + QLatin1Char('/')
                    + QFileInfo(pngPath).completeBaseName() + QStringLiteral(".ico"),
                "ICO");
}

void LauncherBackend::chooseAppIcon(const QString& appId)
{
    const QString languageCode = language();
    const QString title = pick(languageCode, QStringLiteral("Pick an icon for %1"),
                               QStringLiteral("%1 아이콘 선택"))
                              .arg(appId);
    const QString chosen = QFileDialog::getOpenFileName(
        m_dialogParent, title, QString(),
        pick(languageCode, QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp *.ico)"),
             QStringLiteral("이미지 (*.png *.jpg *.jpeg *.bmp *.webp *.ico)")));
    if (chosen.isEmpty())
        return;

    const QImage image(chosen);
    if (image.isNull()) {
        reportError(pick(languageCode, QStringLiteral("Icon"), QStringLiteral("아이콘")),
                    pick(languageCode, QStringLiteral("This file is not a readable image: %1"),
                         QStringLiteral("이미지로 읽을 수 없는 파일입니다: %1"))
                        .arg(QFileInfo(chosen).fileName()));
        return;
    }

    writeAppIcon(appId, image);
    rebuildInstalled();
    if (m_detail.value(QStringLiteral("appId")).toString() == appId) {
        m_detail.insert(QStringLiteral("iconUrl"), appIconUrl(appId));
        emit detailChanged();
    }
    setStatus(pick(languageCode, QStringLiteral("Icon updated for %1"),
                   QStringLiteral("%1 아이콘을 바꿨습니다"))
                  .arg(appId));
}

void LauncherBackend::resetAppIcon(const QString& appId)
{
    const QString pngPath = appIconFile(appId);
    QFile::remove(pngPath);
    QFile::remove(QFileInfo(pngPath).absolutePath() + QLatin1Char('/')
                  + QFileInfo(pngPath).completeBaseName() + QStringLiteral(".ico"));
    rebuildInstalled();
    if (m_detail.value(QStringLiteral("appId")).toString() == appId) {
        m_detail.insert(QStringLiteral("iconUrl"), QString());
        emit detailChanged();
    }
    setStatus(pick(language(), QStringLiteral("Default icon restored for %1"),
                   QStringLiteral("%1 아이콘을 기본값으로 되돌렸습니다"))
                  .arg(appId));
}

void LauncherBackend::createShortcut(const QString& appId)
{
    const QString languageCode = language();
    auto package = m_installer->activePackage(appId);
    if (package.isError()) {
        reportError(pick(languageCode, QStringLiteral("Shortcut"), QStringLiteral("바로가기")),
                    package.error().message);
        return;
    }

    const core::RegistryEntry* entry = m_registry->find(appId);
    const QString name = entry && !entry->name.isEmpty() ? entry->name : package.value().name;
    const QString iconFromUser = QFileInfo(appIconFile(appId)).absolutePath()
                                 + QLatin1Char('/') + QFileInfo(appIconFile(appId)).completeBaseName()
                                 + QStringLiteral(".ico");
    const QString icon = QFileInfo::exists(iconFromUser) ? iconFromUser : QString();

    QString error;
    const QString path = system::Shortcut::createOnDesktop(
        name, QCoreApplication::applicationFilePath(),
        {QStringLiteral("--run-app"), appId}, icon,
        QCoreApplication::applicationDirPath(), &error);
    if (path.isEmpty()) {
        reportError(pick(languageCode, QStringLiteral("Could not create a shortcut"),
                         QStringLiteral("바로가기를 만들지 못했습니다")),
                    error);
        return;
    }

    setStatus(pick(languageCode, QStringLiteral("Shortcut created: %1"),
                   QStringLiteral("바로가기를 만들었습니다: %1"))
                  .arg(QFileInfo(path).fileName()));
    QMessageBox box(m_dialogParent);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(pick(languageCode, QStringLiteral("Shortcut created"),
                            QStringLiteral("바로가기 생성")));
    box.setText(pick(languageCode, QStringLiteral("A desktop shortcut was created."),
                     QStringLiteral("바탕 화면에 바로가기를 만들었습니다.")));
    box.setInformativeText(QFileInfo(path).fileName());
    box.setDetailedText(path);
    box.exec();
}

void LauncherBackend::activateRuntime(const QString& version)
{
    const QString languageCode = language();
    auto activated = m_runtime->activate(version);
    if (activated.isError()) {
        reportError(pick(languageCode, QStringLiteral("Activation failed"),
                         QStringLiteral("활성화 실패")),
                    activated.error().message);
        return;
    }
    refreshRuntimeState();
    setStatus(pick(languageCode, QStringLiteral("Runtime v%1 activated"),
                   QStringLiteral("런타임 v%1 활성화"))
                  .arg(version));
}

void LauncherBackend::removeRuntime(const QString& version)
{
    const QString languageCode = language();
    auto removed = m_runtime->uninstall(version);
    if (removed.isError()) {
        reportError(pick(languageCode, QStringLiteral("Could not remove runtime"),
                         QStringLiteral("런타임 제거 실패")),
                    removed.error().message);
        return;
    }
    refreshRuntimeState();
    setStatus(pick(languageCode, QStringLiteral("Runtime v%1 removed"),
                   QStringLiteral("런타임 v%1 제거"))
                  .arg(version));
}

void LauncherBackend::registerRuntimeFolder()
{
    const QString languageCode = language();
    const QString directory = QFileDialog::getExistingDirectory(
        m_dialogParent,
        pick(languageCode, QStringLiteral("Select a NAPP Web Runtime folder"),
             QStringLiteral("NAPP Web Runtime 폴더 선택")),
        QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (directory.isEmpty())
        return;

    auto registered = m_runtime->registerFromDirectory(directory);
    if (registered.isError()) {
        reportError(pick(languageCode, QStringLiteral("Runtime registration failed"),
                         QStringLiteral("런타임 등록 실패")),
                    registered.error().message);
        return;
    }
    refreshRuntimeState();
    setStatus(pick(languageCode, QStringLiteral("Runtime v%1 registered"),
                   QStringLiteral("런타임 v%1 등록"))
                  .arg(registered.value().version));
}

void LauncherBackend::openRuntimeFolder()
{
    QDir().mkpath(m_paths.runtimeDir());
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_paths.runtimeDir()));
}

QString LauncherBackend::addRepository(const QString& name, const QString& baseUrl)
{
    auto added = m_repository->addRepository(name, baseUrl);
    if (added.isError())
        return added.error().message;
    rebuildRepositories();
    m_repository->refresh();
    return {};
}

QString LauncherBackend::removeRepository(const QString& id)
{
    auto removed = m_repository->removeRepository(id);
    if (removed.isError())
        return removed.error().message;
    rebuildRepositories();
    m_repository->refresh();
    return {};
}

void LauncherBackend::toggleRepository(const QString& id)
{
    for (const store::RepositoryConfig& repository : m_repository->repositories()) {
        if (repository.id == id) {
            m_repository->setRepositoryEnabled(id, !repository.enabled);
            break;
        }
    }
    rebuildRepositories();
    m_repository->refresh();
}

void LauncherBackend::setTheme(const QString& mode)
{
    if (!m_settings)
        return;
    m_settings->setTheme(settings::themeFromString(mode));
    applyNativeTheme();
    updateLaunchEnvironment();
    emit themeChanged();
}

void LauncherBackend::setAccent(const QString& accent)
{
    if (!m_settings)
        return;
    if (!accentIds().contains(accent))
        return;
    m_settings->setAccent(accent);
    applyNativeTheme();
    updateLaunchEnvironment();
    emit themeChanged();
}

void LauncherBackend::setLanguage(const QString& language)
{
    if (!m_settings)
        return;
    const QString normalized = language.startsWith(QStringLiteral("ko")) ? QStringLiteral("ko")
                                                                        : QStringLiteral("en");
    if (m_settings->language() == normalized)
        return;
    m_settings->setLanguage(normalized);
    buildLicenses();
    rebuildStore();
    rebuildInstalled();
    rebuildUpdates();
    rebuildRuntimes();
    refreshRuntimeState();
    m_storeStatus = QStringLiteral("%1  ·  %2")
                        .arg(appCountText(static_cast<int>(m_repository->catalog().size()),
                                          normalized),
                             repositoryCountText(
                                 static_cast<int>(m_repository->repositories().size()),
                                 normalized));
    emit storeStatusChanged();
    updateLaunchEnvironment();
    emit languageChanged();
}

void LauncherBackend::setLibraryQuery(const QString& query)
{
    if (m_libraryQuery == query)
        return;
    m_libraryQuery = query;
    rebuildInstalled();
}

void LauncherBackend::setLibraryFilter(const QString& filter)
{
    if (m_libraryFilter == filter)
        return;
    m_libraryFilter = filter;
    rebuildInstalled();
}

void LauncherBackend::setLibrarySort(int sort)
{
    if (m_librarySort == sort)
        return;
    m_librarySort = sort;
    rebuildInstalled();
}

void LauncherBackend::openExternal(const QString& url)
{
    const QUrl target(url);
    if (target.scheme() != QStringLiteral("https") && target.scheme() != QStringLiteral("http"))
        return;
    QDesktopServices::openUrl(target);
}

void LauncherBackend::updateLaunchEnvironment()
{
    if (!m_launcher)
        return;
    core::AppLauncher::Environment environment;
    environment.language = language();
    environment.dark = darkMode();
    environment.accent = accent();
    environment.repositoryUrl = m_repository ? m_repository->primaryBaseUrl() : QString();
    m_launcher->setEnvironment(environment);
}

void LauncherBackend::applyNativeTheme()
{
    auto* application = qobject_cast<QApplication*>(QApplication::instance());
    if (!application)
        return;
    const bool pinned = m_settings && m_settings->theme() != settings::ThemeMode::System;
    napp::ui::applyTheme(*application, darkMode() ? ColorScheme::Dark : ColorScheme::Light,
                         accent(), pinned);
}

bool LauncherBackend::askPermissions(const QString& headline, const QStringList& permissions,
                                     bool alreadyGrantedHint)
{
    Q_UNUSED(alreadyGrantedHint);
    const QString lc = language();

    QStringList lines;
    for (const QString& permission : permissions)
        lines.append(QStringLiteral("· %1 — %2").arg(permission, permissionDescription(permission, lc)));
    if (lines.isEmpty())
        lines.append(pick(lc, QStringLiteral("This app requests no permissions."),
                          QStringLiteral("이 앱은 권한을 요청하지 않습니다.")));

    // The request also goes out as an OS notification so it is noticed when the
    // launcher is in the background (e.g. after double-clicking a .ipkg).
    // 런처가 백그라운드일 때(.ipkg 더블클릭 직후 등)도 알아차리도록 OS 알림으로도 보낸다.
    toast(pick(lc, QStringLiteral("Permission request"), QStringLiteral("권한 요청")), headline, 1);

    // Top-level native dialog, not tied to the QML window.
    // QML 창에 묶이지 않는 최상위 네이티브 대화상자
    QMessageBox box(nullptr);
    box.setWindowTitle(pick(lc, QStringLiteral("NAPP — Permission request"),
                            QStringLiteral("NAPP — 권한 요청")));
    box.setWindowIcon(QIcon(QStringLiteral(":/branding/napp-icon-256.png")));
    box.setIcon(QMessageBox::Question);
    box.setText(headline);
    box.setInformativeText(lines.join(QLatin1Char('\n')));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    box.button(QMessageBox::Yes)->setText(pick(lc, QStringLiteral("Allow"), QStringLiteral("허용")));
    box.button(QMessageBox::No)->setText(pick(lc, QStringLiteral("Deny"), QStringLiteral("거부")));
    box.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    return box.exec() == QMessageBox::Yes;
}

void LauncherBackend::toast(const QString& title, const QString& message, int level)
{
    if (!m_notifier)
        return;
    m_notifier->notify(title, message,
                       level == 2 ? system::Notifier::Level::Error
                                  : (level == 1 ? system::Notifier::Level::Warning
                                                : system::Notifier::Level::Info));
}

void LauncherBackend::reportError(const QString& title, const QString& message)
{
    qWarning("%s: %s", qPrintable(title), qPrintable(message));
    QMessageBox::warning(m_dialogParent, title, message);
}

} // namespace napp::ui
