#pragma once

#include <QObject>
#include <QString>

class QSystemTrayIcon;

namespace napp::system {

// OS-level notifications (Windows toast via the tray icon). // 운영체제 알림 (트레이 아이콘 기반 토스트)
// Everything the launcher wants to tell the user goes through here instead of
// an in-app banner. // 런처가 사용자에게 알리는 모든 것은 앱 내부 배너 대신 여기로 간다
class Notifier : public QObject
{
    Q_OBJECT

public:
    explicit Notifier(QObject* parent = nullptr);
    ~Notifier() override;

    enum class Level { Info, Warning, Error };

    // Shows an OS notification. Safe to call before the window is visible.
    // OS 알림을 띄운다. 창이 보이기 전에도 호출해도 안전하다.
    void notify(const QString& title, const QString& message, Level level = Level::Info);

signals:
    // Emitted when the user clicks the tray icon or the toast.
    // 사용자가 트레이 아이콘이나 토스트를 클릭하면 발생
    void activated();

private:
    QSystemTrayIcon* m_tray = nullptr;
};

} // namespace napp::system
