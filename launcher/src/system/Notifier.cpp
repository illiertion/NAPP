#include "Notifier.h"

#include <QIcon>
#include <QSystemTrayIcon>

namespace napp::system {

Notifier::Notifier(QObject* parent)
    : QObject(parent)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/branding/napp-icon-256.png")), this);
    m_tray->setToolTip(QStringLiteral("NAPP"));
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                    emit activated();
            });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, &Notifier::activated);
    m_tray->show();
}

Notifier::~Notifier()
{
    // Remove the tray icon right away so it does not linger after exit.
    // 종료 후 트레이 아이콘이 남지 않게 바로 제거
    if (m_tray)
        m_tray->hide();
}

void Notifier::notify(const QString& title, const QString& message, Level level)
{
    if (!m_tray)
        return;

    QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;
    if (level == Level::Warning)
        icon = QSystemTrayIcon::Warning;
    else if (level == Level::Error)
        icon = QSystemTrayIcon::Critical;

    m_tray->showMessage(title, message, icon, 6000);
}

} // namespace napp::system
