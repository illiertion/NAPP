#pragma once

#include "ui/MapListModel.h"

#include "core/AppRegistry.h"
#include "napp/core/paths.h"
#include "napp/ipkg/installer.h"

#include <QHash>
#include <QObject>
#include <QImage>
#include <QVariantList>
#include <QVariantMap>

class QWidget;

namespace napp::core {
class AppLauncher;
class InstallService;
class RuntimeManager;
}

namespace napp::security {
class PermissionStore;
}

namespace napp::settings {
class Settings;
}

namespace napp::system {
class Notifier;
}

namespace napp::store {
class RepositoryService;
}

namespace napp::ui {

// Everything QML needs: read-only list models, page selection, theme/language
// state and invokable actions. Native dialogs stay in Qt Widgets (the shell),
// so QML only describes the interface.
class LauncherBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractListModel* storeModel READ storeModel CONSTANT)
    Q_PROPERTY(QAbstractListModel* installedModel READ installedModel CONSTANT)
    Q_PROPERTY(QAbstractListModel* updatesModel READ updatesModel CONSTANT)
    Q_PROPERTY(QAbstractListModel* runtimeModel READ runtimeModel CONSTANT)
    Q_PROPERTY(QAbstractListModel* repositoryModel READ repositoryModel CONSTANT)
    Q_PROPERTY(QAbstractListModel* licensesModel READ licensesModel CONSTANT)

    Q_PROPERTY(int storeCount READ storeCount NOTIFY countsChanged)
    Q_PROPERTY(int installedCount READ installedCount NOTIFY countsChanged)
    Q_PROPERTY(int libraryCount READ libraryCount NOTIFY libraryChanged)
    Q_PROPERTY(int updatesCount READ updatesCount NOTIFY countsChanged)
    Q_PROPERTY(int runtimeCount READ runtimeCount NOTIFY countsChanged)
    Q_PROPERTY(int repositoryCount READ repositoryCount NOTIFY countsChanged)

    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(bool darkMode READ darkMode NOTIFY themeChanged)
    Q_PROPERTY(QString themeMode READ themeMode NOTIFY themeChanged)
    Q_PROPERTY(QString accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(bool checkUpdates READ checkUpdates WRITE setCheckUpdates NOTIFY settingsChanged)
    Q_PROPERTY(bool associateIpkg READ associateIpkg WRITE setAssociateIpkg NOTIFY settingsChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString storeStatusText READ storeStatusText NOTIFY storeStatusChanged)
    Q_PROPERTY(QString runtimeStatusText READ runtimeStatusText NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(bool detailVisible READ detailVisible WRITE setDetailVisible NOTIFY detailChanged)

    // Library filter/sort (the Library screen owns these controls).
    Q_PROPERTY(QString libraryQuery READ libraryQuery NOTIFY libraryChanged)
    Q_PROPERTY(QString libraryFilter READ libraryFilter NOTIFY libraryChanged)
    Q_PROPERTY(int librarySort READ librarySort NOTIFY libraryChanged)

    // About screen: build facts, filled in from the running binary.
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString buildCompiler READ buildCompiler CONSTANT)
    Q_PROPERTY(QString buildDate READ buildDate CONSTANT)
    Q_PROPERTY(QString cefVersion READ cefVersion NOTIFY runtimeStatusChanged)
    Q_PROPERTY(QString platformName READ platformName CONSTANT)

public:
    LauncherBackend(Paths paths, store::RepositoryService* repository, core::AppRegistry* registry,
                    ipkg::Installer* installer, core::RuntimeManager* runtime,
                    core::AppLauncher* launcher, core::InstallService* installService,
                    settings::Settings* settings, security::PermissionStore* permissions,
                    system::Notifier* notifier, QWidget* dialogParent,
                    QObject* parent = nullptr);

    QAbstractListModel* storeModel() { return &m_storeModel; }
    QAbstractListModel* installedModel() { return &m_installedModel; }
    QAbstractListModel* updatesModel() { return &m_updatesModel; }
    QAbstractListModel* runtimeModel() { return &m_runtimeModel; }
    QAbstractListModel* repositoryModel() { return &m_repositoryModel; }
    QAbstractListModel* licensesModel() { return &m_licensesModel; }

    // QAbstractItemModel exposes rowCount() as a method, not a QML property, so
    // the screens bind to these instead of model.count.
    int storeCount() const { return m_storeModel.count(); }
    int installedCount() const { return m_installedTotal; }
    int libraryCount() const { return m_installedModel.count(); }
    int updatesCount() const { return m_updatesModel.count(); }
    int runtimeCount() const { return m_runtimeModel.count(); }
    int repositoryCount() const { return m_repositoryModel.count(); }

    int currentPage() const { return m_currentPage; }
    void setCurrentPage(int page);

    bool darkMode() const;
    QString themeMode() const;
    QString accent() const;
    QString language() const;

    bool checkUpdates() const;
    // Q_INVOKABLE: SettingsPage.qml calls these as functions. Without it the toggle
    // threw "not a function" and the setting was never saved.
    // Q_INVOKABLE: SettingsPage.qml 이 함수로 호출한다. 없으면 "not a function" 오류가 나서
    // 설정이 저장되지 않았다.
    Q_INVOKABLE void setCheckUpdates(bool enabled);

    bool associateIpkg() const;
    Q_INVOKABLE void setAssociateIpkg(bool enabled);

    QString statusText() const { return m_statusText; }
    bool busy() const { return m_busy; }
    qreal progress() const { return m_progress; }
    QString storeStatusText() const { return m_storeStatus; }
    QString runtimeStatusText() const { return m_runtimeStatus; }
    QVariantMap detail() const { return m_detail; }
    bool detailVisible() const { return m_detailVisible; }
    void setDetailVisible(bool visible);

    QString libraryQuery() const { return m_libraryQuery; }
    QString libraryFilter() const { return m_libraryFilter; }
    int librarySort() const { return m_librarySort; }

    QString appVersion() const;
    QString qtVersion() const;
    QString buildCompiler() const;
    QString buildDate() const;
    QString cefVersion() const;
    QString platformName() const;

    void setStatus(const QString& message);

    // Opens a local .ipkg: verifies it, asks the user to approve the requested
    // permissions in a native OS dialog, then installs. Used by the file dialog,
    // by double-clicking a .ipkg and by a second launch forwarding its argument.
    // 로컬 .ipkg 를 연다: 검증 → 요청 권한을 OS 네이티브 대화상자로 승인받음 → 설치.
    // 파일 선택, .ipkg 더블클릭, 두 번째 실행의 인자 전달 모두 이 경로를 쓴다.
    void openPackageFile(const QString& path);

    Q_INVOKABLE QStringList categories() const;
    Q_INVOKABLE void refreshStore();
    Q_INVOKABLE void setStoreQuery(const QString& query);
    Q_INVOKABLE void setStoreCategory(const QString& category);

    Q_INVOKABLE void showAppDetails(const QString& appId);
    Q_INVOKABLE void installApp(const QString& appId, const QString& version);
    Q_INVOKABLE void installFromFile();
    Q_INVOKABLE void launchApp(const QString& appId);
    Q_INVOKABLE void stopApp(const QString& appId);
    Q_INVOKABLE void removeApp(const QString& appId);
    Q_INVOKABLE void openDataFolder(const QString& appId);
    Q_INVOKABLE void editPermissions(const QString& appId);

    // Per-app icon the user picked. Icons live in config/icons/<appId>.png so
    // they survive app updates (the package itself is replaced).
    // 사용자가 고른 앱 아이콘. config/icons/<appId>.png 에 두어 앱 업데이트에도 유지된다.
    Q_INVOKABLE void chooseAppIcon(const QString& appId);
    Q_INVOKABLE void resetAppIcon(const QString& appId);
    Q_INVOKABLE void createShortcut(const QString& appId);

    Q_INVOKABLE void activateRuntime(const QString& version);
    Q_INVOKABLE void removeRuntime(const QString& version);
    Q_INVOKABLE void registerRuntimeFolder();
    Q_INVOKABLE void openRuntimeFolder();

    Q_INVOKABLE QString addRepository(const QString& name, const QString& baseUrl);
    Q_INVOKABLE QString removeRepository(const QString& id);
    Q_INVOKABLE void toggleRepository(const QString& id);

    Q_INVOKABLE void setTheme(const QString& mode);
    Q_INVOKABLE void setAccent(const QString& accent);
    Q_INVOKABLE void setLanguage(const QString& language);

    Q_INVOKABLE void setLibraryQuery(const QString& query);
    Q_INVOKABLE void setLibraryFilter(const QString& filter);
    Q_INVOKABLE void setLibrarySort(int sort);

    Q_INVOKABLE void openExternal(const QString& url);

signals:
    void countsChanged();
    void currentPageChanged();
    void themeChanged();
    void languageChanged();
    void libraryChanged();
    void settingsChanged();
    void statusTextChanged();
    void busyChanged();
    void progressChanged();
    void storeStatusChanged();
    void runtimeStatusChanged();
    void detailChanged();

private:
    void rebuildStore();
    void rebuildInstalled();
    void rebuildUpdates();
    void rebuildRuntimes();
    void rebuildRepositories();
    void buildLicenses();
    void refreshRuntimeState();
    bool hasUpdate(const core::RegistryEntry& entry) const;
    QString appIconFile(const QString& appId) const;
    QString appIconUrl(const QString& appId) const;
    QString appIconDirectory() const;
    void writeAppIcon(const QString& appId, const QImage& image);
    void reportError(const QString& title, const QString& message);
    bool askPermissions(const QString& headline, const QStringList& permissions,
                        bool alreadyGrantedHint);
    void toast(const QString& title, const QString& message, int level = 0);
    void applyNativeTheme();
    void updateLaunchEnvironment();

    Paths m_paths;
    store::RepositoryService* m_repository = nullptr;
    core::AppRegistry* m_registry = nullptr;
    ipkg::Installer* m_installer = nullptr;
    core::RuntimeManager* m_runtime = nullptr;
    core::AppLauncher* m_launcher = nullptr;
    core::InstallService* m_installService = nullptr;
    settings::Settings* m_settings = nullptr;
    security::PermissionStore* m_permissions = nullptr;
    system::Notifier* m_notifier = nullptr;
    QWidget* m_dialogParent = nullptr;
    // Permissions the user approved before an install started; applied once the
    // package is registered. // 설치 시작 전에 승인한 권한, 등록이 끝나면 적용
    QHash<QString, QStringList> m_pendingGrants;

    MapListModel m_storeModel;
    MapListModel m_installedModel;
    MapListModel m_updatesModel;
    MapListModel m_runtimeModel;
    MapListModel m_repositoryModel;
    MapListModel m_licensesModel;

    QString m_storeQuery;
    QString m_storeCategory;
    QString m_libraryQuery;
    QString m_libraryFilter;
    int m_librarySort = 0;
    QString m_statusText;
    QString m_storeStatus;
    QString m_runtimeStatus;
    QVariantMap m_detail;
    QVariantList m_detailVersions;
    bool m_detailVisible = false;
    int m_currentPage = 0;
    int m_installedTotal = 0;
    bool m_busy = false;
    qreal m_progress = 0.0;
};

} // namespace napp::ui
