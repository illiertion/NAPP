#include "napp/ipkg/installer.h"

#include "napp/core/json.h"
#include "napp/core/logging.h"
#include "napp/ipkg/manifest_parser.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QUuid>

namespace napp::ipkg {
namespace {

constexpr auto kMarkerFile = "napp-package.json";

bool removeRecursively(const QString& path)
{
    if (!QFileInfo::exists(path))
        return true;
    return QDir(path).removeRecursively();
}

} // namespace

Installer::Installer(Paths paths, Limits limits)
    : m_paths(std::move(paths))
    , m_limits(limits)
    , m_verifier(limits)
{
}

Result<InstalledPackage> Installer::install(const InstallRequest& request)
{
    if (!m_paths.isValid())
        return Result<InstalledPackage>::fail(ErrorCode::Internal, QStringLiteral("데이터 경로가 설정되지 않았습니다"));

    if (!m_paths.ensureLayout())
        return Result<InstalledPackage>::fail(ErrorCode::DiskError, QStringLiteral("데이터 디렉터리를 만들 수 없습니다"));

    auto package = m_verifier.verify(request.packagePath, request.expectedSha256);
    if (!package)
        return package.error();

    const Manifest& manifest = package.value().manifest;

    if (request.allowPlatformCheck) {
        auto platform = m_verifier.checkPlatform(manifest);
        if (!platform)
            return platform.error();
    }

    const QString finalDir = m_paths.appVersionDir(manifest.appId, manifest.version);
    if (QFileInfo::exists(finalDir)) {
        return Result<InstalledPackage>::fail(
            ErrorCode::AppAlreadyInstalled,
            QStringLiteral("이미 설치된 버전입니다: %1 %2").arg(manifest.appId, manifest.version));
    }

    const QString stagingRoot = QDir(m_paths.stagingDir())
                                    .filePath(QStringLiteral("%1-%2-%3")
                                                  .arg(manifest.appId, manifest.version,
                                                       QUuid::createUuid().toString(QUuid::Id128).left(8)));
    QDir().mkpath(stagingRoot);

    auto extracted = extractAll(request.packagePath, stagingRoot);
    if (!extracted) {
        removeRecursively(stagingRoot);
        return extracted.error();
    }

    QJsonObject marker;
    marker.insert(QStringLiteral("appId"), manifest.appId);
    marker.insert(QStringLiteral("version"), manifest.version);
    marker.insert(QStringLiteral("name"), manifest.name);
    marker.insert(QStringLiteral("entry"), manifest.entry);
    marker.insert(QStringLiteral("repositoryId"), request.repositoryId);
    marker.insert(QStringLiteral("sha256"), package.value().sha256);
    marker.insert(QStringLiteral("installedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    QJsonArray permissionsArray;
    for (const QString& permission : manifest.permissions)
        permissionsArray.append(permission);
    marker.insert(QStringLiteral("permissions"), permissionsArray);
    if (!manifest.windowTitle.isEmpty() || manifest.windowWidth > 0 || manifest.windowHeight > 0
        || manifest.windowTray) {
        QJsonObject window;
        if (!manifest.windowTitle.isEmpty())
            window.insert(QStringLiteral("title"), manifest.windowTitle);
        if (manifest.windowWidth > 0)
            window.insert(QStringLiteral("width"), manifest.windowWidth);
        if (manifest.windowHeight > 0)
            window.insert(QStringLiteral("height"), manifest.windowHeight);
        window.insert(QStringLiteral("tray"), manifest.windowTray);
        marker.insert(QStringLiteral("window"), window);
    }

    auto markerWrite = json::writeObjectAtomically(QDir(stagingRoot).filePath(QString::fromLatin1(kMarkerFile)), marker);
    if (!markerWrite) {
        removeRecursively(stagingRoot);
        return markerWrite.error();
    }

    // The trailing component may not exist yet on a fresh app id.
    if (!QDir().mkpath(QFileInfo(finalDir).absolutePath())) {
        removeRecursively(stagingRoot);
        return Result<InstalledPackage>::fail(ErrorCode::DiskError, QStringLiteral("앱 디렉터리를 만들 수 없습니다"));
    }

    if (!QDir().rename(stagingRoot, finalDir)) {
        removeRecursively(stagingRoot);
        return Result<InstalledPackage>::fail(ErrorCode::DiskError,
                                              QStringLiteral("설치 디렉터리를 확정할 수 없습니다: %1").arg(finalDir));
    }

    auto activated = activate(manifest.appId, manifest.version);
    if (!activated) {
        // Keep the extracted files but surface the failure; the caller retries.
        return activated.error();
    }

    qInfo() << "installed" << manifest.appId << manifest.version << "->" << finalDir;
    return describe(manifest.appId, manifest.version);
}

Result<void> Installer::extractAll(const QString& packagePath, const QString& destination) const
{
    ZipReader reader;
    auto opened = reader.open(packagePath, m_limits);
    if (!opened)
        return opened.error();

    for (const ZipEntry& entry : reader.entries()) {
        if (entry.isDirectory)
            continue;

        const QString target = Paths::resolveInside(destination, entry.name);
        if (target.isEmpty()) {
            return Result<void>::fail(ErrorCode::UnsafePath,
                                      QStringLiteral("압축 해제 경로가 설치 디렉터리를 벗어납니다: %1").arg(entry.name));
        }

        auto extracted = reader.extractTo(entry.name, target, m_limits.maxEntrySize);
        if (!extracted)
            return extracted.error();
    }
    return Result<void>::ok();
}

Result<void> Installer::activate(const QString& appId, const QString& version)
{
    if (!QFileInfo::exists(m_paths.appVersionDir(appId, version)))
        return Result<void>::fail(ErrorCode::AppNotFound,
                                  QStringLiteral("설치된 버전이 없습니다: %1 %2").arg(appId, version));

    auto written = json::writeFileAtomically(m_paths.appPointerFile(appId), version.toUtf8());
    if (!written)
        return written.error();
    return Result<void>::ok();
}

Result<QString> Installer::activeVersion(const QString& appId) const
{
    auto bytes = json::readFile(m_paths.appPointerFile(appId), ErrorCode::AppNotFound);
    if (!bytes)
        return bytes.error();
    return QString::fromUtf8(bytes.value()).trimmed();
}

Result<InstalledPackage> Installer::describe(const QString& appId, const QString& version) const
{
    const QString directory = m_paths.appVersionDir(appId, version);
    InstalledPackage installed;
    installed.appId = appId;
    installed.version = version;
    installed.directory = directory;

    auto marker = json::readObjectOrEmpty(QDir(directory).filePath(QString::fromLatin1(kMarkerFile)));
    if (marker) {
        const QJsonObject object = marker.value();
        installed.name = object.value(QStringLiteral("name")).toString();
        installed.repositoryId = object.value(QStringLiteral("repositoryId")).toString();
        installed.entryRelPath = object.value(QStringLiteral("entry")).toString();
        for (const QJsonValue& value : object.value(QStringLiteral("permissions")).toArray())
            installed.permissions.append(value.toString());

        const QJsonObject window = object.value(QStringLiteral("window")).toObject();
        installed.windowTitle = window.value(QStringLiteral("title")).toString();
        installed.windowWidth = window.value(QStringLiteral("width")).toInt();
        installed.windowHeight = window.value(QStringLiteral("height")).toInt();
        installed.windowTray = window.value(QStringLiteral("tray")).toBool();
    }

    // The manifest is the source of truth for anything the marker does not carry
    // (markers written by older launchers have no window block), and for the
    // package icon the launcher draws when the user has not picked one.
    // 마커에 없는 값(구버전 마커에는 window 가 없다)과, 사용자가 따로 고르지 않았을 때
    // 런처가 그릴 패키지 아이콘은 매니페스트에서 읽는다.
    auto manifestBytes = json::readFile(QDir(directory).filePath(QStringLiteral("manifest.json")));
    if (manifestBytes) {
        auto manifest = ManifestParser::parse(manifestBytes.value(), directory);
        if (manifest) {
            if (installed.name.isEmpty())
                installed.name = manifest.value().name;
            if (installed.entryRelPath.isEmpty())
                installed.entryRelPath = manifest.value().entry;
            installed.iconRelPath = manifest.value().iconPath;
            if (installed.windowTitle.isEmpty())
                installed.windowTitle = manifest.value().windowTitle;
            if (installed.windowWidth == 0)
                installed.windowWidth = manifest.value().windowWidth;
            if (installed.windowHeight == 0)
                installed.windowHeight = manifest.value().windowHeight;
            if (!installed.windowTray)
                installed.windowTray = manifest.value().windowTray;
        }
    }

    if (installed.entryRelPath.isEmpty())
        return Result<InstalledPackage>::fail(ErrorCode::EntryFileMissing,
                                              QStringLiteral("설치 정보가 손상되었습니다: %1").arg(directory));

    return installed;
}

Result<InstalledPackage> Installer::activePackage(const QString& appId) const
{
    auto version = activeVersion(appId);
    if (!version)
        return version.error();
    return describe(appId, version.value());
}

Result<void> Installer::remove(const QString& appId, bool purgeData)
{
    const QString appDir = QDir(m_paths.appsDir()).filePath(appId);
    if (!QFileInfo::exists(appDir))
        return Result<void>::fail(ErrorCode::AppNotFound, QStringLiteral("설치된 앱이 없습니다: %1").arg(appId));

    if (!removeRecursively(appDir))
        return Result<void>::fail(ErrorCode::DiskError, QStringLiteral("앱 디렉터리를 삭제할 수 없습니다: %1").arg(appDir));

    if (purgeData)
        removeRecursively(m_paths.dataDir(appId));

    qInfo() << "removed" << appId << "purgeData" << purgeData;
    return Result<void>::ok();
}

Result<QStringList> Installer::installedAppIds() const
{
    QDir apps(m_paths.appsDir());
    if (!apps.exists())
        return QStringList{};

    QStringList ids;
    for (const QString& name : apps.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (QFileInfo::exists(m_paths.appPointerFile(name)))
            ids.append(name);
    }
    return ids;
}

Result<QStringList> Installer::installedVersions(const QString& appId) const
{
    QDir appDir(QDir(m_paths.appsDir()).filePath(appId));
    if (!appDir.exists())
        return QStringList{};

    QStringList versions;
    for (const QString& name : appDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (QFileInfo::exists(QDir(appDir.filePath(name)).filePath(QStringLiteral("manifest.json"))))
            versions.append(name);
    }
    return versions;
}

Result<void> Installer::pruneVersions(const QString& appId, const QString& keepVersion)
{
    auto versions = installedVersions(appId);
    if (!versions)
        return versions.error();

    for (const QString& version : versions.value()) {
        if (version == keepVersion)
            continue;
        if (!removeRecursively(m_paths.appVersionDir(appId, version))) {
            return Result<void>::fail(ErrorCode::DiskError,
                                      QStringLiteral("이전 버전을 삭제할 수 없습니다: %1 %2").arg(appId, version));
        }
    }
    return Result<void>::ok();
}

} // namespace napp::ipkg
