#pragma once

#include "host_client.h"
#include "host_options.h"

#include <QIcon>
#include <QMainWindow>

class QLabel;
class QMenu;
class QSystemTrayIcon;
class QWidget;

namespace napp::runtime {

// One window = one app = one CEF browser inside its own process.
class HostWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit HostWindow(HostOptions options, QWidget* parent = nullptr);
    ~HostWindow() override;

    void start();
    void closeBrowser();

    // Shows an OS notification (Windows toast). Used by the page-side
    // Notification shim and by download completion. UI thread only.
    // OS 알림(Windows 토스트)을 띄운다. 페이지의 Notification shim 과 다운로드 완료에서 사용. UI 스레드 전용.
    void showNotification(const QString& title, const QString& body);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void resizeBrowser();
    void forceQuit();
    // Pushes the app icon into the window class (Alt+Tab, taskbar group, system
    // menu). Windows only; a no-op elsewhere.
    // 앱 아이콘을 창 클래스(Alt+Tab, 작업 표시줄 그룹, 시스템 메뉴)에 반영한다.
    void applyWindowIcon(const QIcon& icon);
    // Builds the tray icon and, when the manifest asked for one, keeps it in the
    // notification area with Open / Quit. Requires the "window" permission.
    // 트레이 아이콘을 만들고, 매니페스트가 원하면 열기/종료 메뉴와 함께 상주시킨다.
    void setUpTray();
    void quitFromTray();
    void restoreWindow();

    HostOptions m_options;
    QIcon m_windowIcon;       // resolved app icon (window/taskbar/tray)
    bool m_iconApplied = false;
    QWidget* m_container = nullptr;
    QLabel* m_overlay = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_trayMenu = nullptr;
    CefRefPtr<NappClient> m_client;
    bool m_browserClosed = false;
    bool m_closing = false;
    bool m_screenHooked = false;
    bool m_trayResident = false;  // manifest asked for a tray icon // 매니페스트가 트레이를 요청
    bool m_quitting = false;      // quitting, not hiding to the tray // 트레이로 숨기지 않고 종료 중
};

} // namespace napp::runtime
