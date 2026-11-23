#pragma once

#include <QString>
#include <QStringList>

namespace napp::runtime {

// Launch arguments supplied by the launcher (see core/AppLauncher.cpp).
struct HostOptions
{
    QString appId;
    QString appName;
    QString appRoot;
    QString entry;            // relative to appRoot, e.g. "app/index.html"
    QString dataDir;          // per-app data root; the CEF profile lives below it
    QStringList permissions;
    QString runtimeConfig;    // runtime.json of the running runtime

    // Optional window preferences from the app manifest. Empty/0 = defaults.
    // 앱 매니페스트의 선택적 창 설정. 비어 있거나 0 이면 기본값.
    QString windowTitle;
    int windowWidth = 0;
    int windowHeight = 0;
    // Absolute path to the image the window/taskbar/tray icon is drawn from.
    // Empty = the napp.ico shipped next to the host.
    // 창·작업 표시줄·트레이 아이콘으로 쓸 이미지의 절대 경로. 비어 있으면 호스트 옆의 napp.ico 를 쓴다.
    QString windowIcon;
    bool tray = false;

    // Launcher environment handed to the page (window.napp). Kept in sync with
    // Settings: apps read the same language/theme the launcher is showing.
    QString language = QStringLiteral("en");
    QString theme = QStringLiteral("dark");   // dark | light
    QString accent = QStringLiteral("blue");  // blue | violet | mint | amber
    QString repositoryUrl;                    // store base URL for on-demand docs

    QString startUrl() const;
    QString profileDir() const;
    QString logFile() const;

    bool isValid() const;
    QString errorMessage() const;
};

// Parses argv without initialising Qt application objects.
HostOptions parseOptions(int argc, char** argv);

} // namespace napp::runtime
