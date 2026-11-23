#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

namespace napp::runtime {

// Everything the launcher knows about the current session, exposed to a page
// as `window.napp`. The values come from the launcher's settings, so an app
// follows the same language, theme and accent without asking the user again.
struct NappEnvironment
{
    QString appId;
    QString appName;
    QString language = QStringLiteral("en");
    QString theme = QStringLiteral("dark");
    QString accent = QStringLiteral("blue");
    QString repositoryUrl;
    QStringList permissions;
    QString runtimeVersion;
    QString apiVersion = QStringLiteral("1.0");

    // Shows an OS notification (set by the host window). Called from CEF's IO
    // thread, so it must only queue work. Empty when no window exists yet.
    // OS 알림을 띄운다 (호스트 창이 설정). CEF IO 스레드에서 호출되므로 작업을 큐에만 넣어야 한다.
    std::function<void(const QString& title, const QString& body)> notify;

    QString accentColor() const;
    bool isDark() const { return theme.compare(QStringLiteral("light"), Qt::CaseInsensitive) != 0; }
};

// JSON document returned by napp://<appId>/__host/info.
QByteArray environmentJson(const NappEnvironment& environment);

// <script> block injected into every HTML document served from napp://.
// It defines window.napp and publishes the palette as CSS custom properties,
// so a plain web app can pick the launcher theme up with one rule.
QByteArray environmentScript(const NappEnvironment& environment);

// Inserts environmentScript() into an HTML document (before </head>), leaving
// documents without a head untouched apart from a prepended script.
QByteArray injectEnvironment(const QByteArray& html, const NappEnvironment& environment);

} // namespace napp::runtime
