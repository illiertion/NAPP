#include "host_options.h"

#include "napp/core/paths.h"

#include <QDir>
#include <QFileInfo>

namespace napp::runtime {

QString HostOptions::startUrl() const
{
    QString path = entry;
    while (path.startsWith(QLatin1Char('/')))
        path.remove(0, 1);
    return QStringLiteral("napp://%1/%2").arg(appId, path);
}

// CEF silently falls back to in-memory storage when its cache paths are not
// absolute, so the profile paths are always resolved to absolute paths.
QString HostOptions::profileDir() const
{
    return QDir(QFileInfo(dataDir).absoluteFilePath()).filePath(QStringLiteral("web"));
}

QString HostOptions::logFile() const
{
    return QDir(QFileInfo(dataDir).absoluteFilePath()).filePath(QStringLiteral("logs/runtime.log"));
}

bool HostOptions::isValid() const
{
    return errorMessage().isEmpty();
}

QString HostOptions::errorMessage() const
{
    if (appId.isEmpty())
        return QStringLiteral("--app-id 가 필요합니다");
    if (appRoot.isEmpty() || !QFileInfo::exists(appRoot))
        return QStringLiteral("--app-root 가 존재하지 않습니다: %1").arg(appRoot);
    if (entry.isEmpty())
        return QStringLiteral("--entry 가 필요합니다");
    if (dataDir.isEmpty())
        return QStringLiteral("--data-dir 가 필요합니다");

    if (!napp::Paths::isInside(appRoot, QDir(appRoot).filePath(entry)))
        return QStringLiteral("--entry 가 앱 루트를 벗어납니다: %1").arg(entry);

    if (!QFileInfo::exists(QDir(appRoot).filePath(entry)))
        return QStringLiteral("--entry 파일이 없습니다: %1").arg(entry);

    return {};
}

HostOptions parseOptions(int argc, char** argv)
{
    HostOptions options;
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromUtf8(argv[index]);
        auto next = [&](QString& target) {
            if (index + 1 < argc)
                target = QString::fromUtf8(argv[++index]);
        };

        if (argument == QStringLiteral("--app-id")) {
            next(options.appId);
        } else if (argument == QStringLiteral("--app-name")) {
            next(options.appName);
        } else if (argument == QStringLiteral("--app-root")) {
            next(options.appRoot);
        } else if (argument == QStringLiteral("--entry")) {
            next(options.entry);
        } else if (argument == QStringLiteral("--data-dir")) {
            next(options.dataDir);
        } else if (argument == QStringLiteral("--permissions")) {
            QString value;
            next(value);
            options.permissions = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
        } else if (argument == QStringLiteral("--language")) {
            next(options.language);
        } else if (argument == QStringLiteral("--theme")) {
            next(options.theme);
        } else if (argument == QStringLiteral("--accent")) {
            next(options.accent);
        } else if (argument == QStringLiteral("--repository-url")) {
            next(options.repositoryUrl);
        } else if (argument == QStringLiteral("--window-title")) {
            next(options.windowTitle);
        } else if (argument == QStringLiteral("--window-width")) {
            QString value;
            next(value);
            options.windowWidth = value.toInt();
        } else if (argument == QStringLiteral("--window-height")) {
            QString value;
            next(value);
            options.windowHeight = value.toInt();
        } else if (argument == QStringLiteral("--window-icon")) {
            next(options.windowIcon);
        } else if (argument == QStringLiteral("--tray")) {
            QString value;
            next(value);
            options.tray = value == QStringLiteral("1");
        } else if (argument.startsWith(QStringLiteral("--runtime-config="))) {
            options.runtimeConfig = argument.mid(int(QStringLiteral("--runtime-config=").size()));
        }
    }
    return options;
}

} // namespace napp::runtime
