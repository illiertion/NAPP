#pragma once

#include <QString>
#include <QStringList>

namespace napp::ipkg {

// Parsed manifest.json (format_version 1). Fields mirror
// specifications/schemas/manifest.schema.json exactly.
struct Manifest
{
    int formatVersion = 0;

    QString appId;
    QString name;
    QString version;
    QString description;
    QString author;
    QString license;
    QString homepage;
    QStringList keywords;

    QString runtimeType;
    QString minimumRuntimeVersion;
    QString minimumSdkVersion;

    QString entry;
    QStringList platforms;
    QStringList permissions;

    // Optional "window" block: the title, opening size, the window/taskbar/tray
    // icon and whether the runtime keeps a tray icon. Empty/0 means "use the
    // runtime defaults".
    // 선택 사항인 "window" 블록: 창 제목, 시작 크기, 창/작업 표시줄/트레이 아이콘,
    // 트레이 아이콘 유지 여부. 비어 있거나 0 이면 런타임 기본값을 쓴다.
    QString windowTitle;
    int windowWidth = 0;
    int windowHeight = 0;
    QString windowIconPath;   // relative to the package root; empty = NAPP icon
    bool windowTray = false;

    QString iconPath;
    QString previewPath;
    QStringList screenshots;

    bool isValid() const { return formatVersion == 1 && !appId.isEmpty() && !entry.isEmpty(); }
};

QStringList knownPermissions();
QStringList knownPlatforms();
bool isKnownPermission(const QString& permission);
bool isKnownPlatform(const QString& platform);

} // namespace napp::ipkg
