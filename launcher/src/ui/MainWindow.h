#pragma once

#include "core/AppRegistry.h"
#include "napp/core/paths.h"

#include <QImage>
#include <QMainWindow>

#include <memory>

class QQuickWidget;

namespace napp::core {
class AppLauncher;
class InstallService;
class RuntimeManager;
}

namespace napp::ipkg {
class Installer;
}

namespace napp::security {
class PermissionStore;
}

namespace napp::settings {
class Settings;
}

namespace napp::store {
class HttpClient;
class RepositoryService;
}

namespace napp::system {
class Notifier;
}

namespace napp::ui {

class LauncherBackend;

// Widgets shell only: the window, native dialogs and the service graph live
// here, while every screen is QML (launcher/qml) rendered by a QQuickWidget.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(Paths paths, QWidget* parent = nullptr);
    ~MainWindow() override;

    // Debug helper (`--ui-snapshot`): renders the QML scene to an image so the
    // layout can be inspected without a desktop screenshot.
    QImage snapshot();
    void showPage(int index);

    // Brings the window to the front (second launch / tray click / toast click).
    // 창을 앞으로 가져온다 (두 번째 실행 / 트레이 클릭 / 토스트 클릭)
    void raiseAndActivate();

    // Opens the first .ipkg in `files` through the install flow; an empty list
    // only focuses the window. // 첫 번째 .ipkg 를 설치 흐름으로 연다. 비어 있으면 포커스만
    void openFiles(const QStringList& files);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildServices();
    void deferredStart();
    void prepareBundledRuntime();
    void restoreOrCenter();
    void buildUi();
    void applyTheme();
    void reconcileRegistry();

    Paths m_paths;
    std::unique_ptr<ipkg::Installer> m_installer;

    store::HttpClient* m_http = nullptr;
    store::RepositoryService* m_repository = nullptr;
    core::AppRegistry* m_registry = nullptr;
    core::RuntimeManager* m_runtime = nullptr;
    core::AppLauncher* m_launcher = nullptr;
    core::InstallService* m_installService = nullptr;
    settings::Settings* m_settings = nullptr;
    security::PermissionStore* m_permissions = nullptr;
    system::Notifier* m_notifier = nullptr;

    LauncherBackend* m_backend = nullptr;
    QQuickWidget* m_quick = nullptr;
};

} // namespace napp::ui
