#include "RuntimeManager.h"

#include "napp/core/json.h"
#include "napp/core/logging.h"
#include "napp/core/platform.h"
#include "napp/core/semver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>

namespace napp::core {
namespace {

bool copyRecursively(const QString& source, const QString& destination, bool overwriteExisting)
{
    QDir sourceDir(source);
    if (!sourceDir.exists())
        return false;

    QDir destinationDir(destination);
    if (!destinationDir.exists() && !QDir().mkpath(destination))
        return false;

    const QFileInfoList entries = sourceDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        const QString target = QDir(destination).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyRecursively(entry.absoluteFilePath(), target, overwriteExisting))
                return false;
        } else {
            if (QFileInfo::exists(target) && !overwriteExisting)
                return false;
            if (QFileInfo::exists(target) && overwriteExisting)
                QFile::remove(target);
            if (!QFile::copy(entry.absoluteFilePath(), target))
                return false;
        }
    }
    return true;
}

bool sameRuntime(const InstalledRuntime& left, const InstalledRuntime& right)
{
    return left.version == right.version && left.chromiumVersion == right.chromiumVersion
        && left.platform == right.platform && left.directory == right.directory
        && left.hostExecutable == right.hostExecutable && left.valid == right.valid
        && left.problem == right.problem;
}

bool sameRuntimes(const QList<InstalledRuntime>& left, const QList<InstalledRuntime>& right)
{
    if (left.size() != right.size())
        return false;
    for (int index = 0; index < left.size(); ++index)
        if (!sameRuntime(left.at(index), right.at(index)))
            return false;
    return true;
}

} // namespace

RuntimeManager::RuntimeManager(Paths paths, QObject* parent)
    : QObject(parent)
    , m_paths(std::move(paths))
{
}

QString RuntimeManager::hostExecutableName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("napp-runtime-host.exe");
#else
    return QStringLiteral("napp-runtime-host");
#endif
}

QString RuntimeManager::runtimeManifestName()
{
    return QStringLiteral("runtime.json");
}

void RuntimeManager::scan()
{
    // Handlers connected to changed() call refreshRuntimeState(), which scans
    // again. Emitting unconditionally therefore recursed until the stack was
    // exhausted, so the signal only goes out when the result actually differs.
    // changed() 슬롯이 다시 scan() 을 부르므로, 결과가 실제로 바뀔 때만 시그널을 보낸다.
    const QList<InstalledRuntime> previous = m_installed;
    m_installed.clear();

    QDir runtimeRoot(m_paths.runtimeDir());
    if (!runtimeRoot.exists()) {
        if (!previous.isEmpty())
            emit changed();
        return;
    }

    const QStringList versions = runtimeRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& version : versions) {
        const QString directory = runtimeRoot.filePath(version);
        InstalledRuntime runtime;
        runtime.version = version;
        runtime.directory = directory;

        auto document = json::readObjectOrEmpty(QDir(directory).filePath(runtimeManifestName()));
        if (!document) {
            runtime.problem = QStringLiteral("runtime.json 을 읽을 수 없습니다");
            m_installed.append(runtime);
            continue;
        }

        const QJsonObject object = document.value();
        if (object.value(QStringLiteral("format")).toString() != QStringLiteral("NAPP-RUNTIME")) {
            runtime.problem = QStringLiteral("runtime.json 형식이 올바르지 않습니다");
            m_installed.append(runtime);
            continue;
        }

        runtime.chromiumVersion = object.value(QStringLiteral("chromium_version")).toString();
        runtime.platform = object.value(QStringLiteral("platform")).toString();
        const QString host = object.value(QStringLiteral("host")).toString(RuntimeManager::hostExecutableName());
        runtime.hostExecutable = QDir(directory).filePath(host);

        if (!SemVer::isValid(runtime.version)) {
            runtime.problem = QStringLiteral("런타임 버전이 semver가 아닙니다");
        } else if (!runtime.platform.isEmpty() && runtime.platform != currentPlatformId()) {
            runtime.problem = QStringLiteral("이 플랫폼(%1)용 런타임이 아닙니다").arg(runtime.platform);
        } else if (!QFileInfo::exists(runtime.hostExecutable)) {
            runtime.problem = QStringLiteral("런타임 실행 파일이 없습니다: %1").arg(runtime.hostExecutable);
        } else {
            runtime.valid = true;
        }

        m_installed.append(runtime);
    }

    if (!sameRuntimes(previous, m_installed))
        emit changed();
}

const InstalledRuntime* RuntimeManager::find(const QString& version) const
{
    for (const InstalledRuntime& runtime : m_installed) {
        if (runtime.version == version)
            return &runtime;
    }
    return nullptr;
}

QString RuntimeManager::activeVersion() const
{
    auto bytes = json::readFile(QDir(m_paths.configRoot()).filePath(QStringLiteral("runtime-current")));
    if (!bytes)
        return {};
    return QString::fromUtf8(bytes.value()).trimmed();
}

const InstalledRuntime* RuntimeManager::active() const
{
    const QString version = activeVersion();
    if (version.isEmpty())
        return nullptr;
    const InstalledRuntime* runtime = find(version);
    return runtime && runtime->valid ? runtime : nullptr;
}

Result<void> RuntimeManager::writeActiveVersion(const QString& version) const
{
    return json::writeFileAtomically(QDir(m_paths.configRoot()).filePath(QStringLiteral("runtime-current")),
                                     version.toUtf8());
}

Result<void> RuntimeManager::activate(const QString& version)
{
    const InstalledRuntime* runtime = find(version);
    if (!runtime)
        return Result<void>::fail(ErrorCode::RuntimeMissing,
                                  QStringLiteral("설치되지 않은 런타임 버전입니다: %1").arg(version));
    if (!runtime->valid)
        return Result<void>::fail(ErrorCode::RuntimeUnsupported, runtime->problem);

    auto written = writeActiveVersion(version);
    if (written)
        emit changed();
    return written;
}

Result<void> RuntimeManager::uninstall(const QString& version)
{
    const InstalledRuntime* runtime = find(version);
    if (!runtime)
        return Result<void>::fail(ErrorCode::RuntimeMissing,
                                  QStringLiteral("설치되지 않은 런타임 버전입니다: %1").arg(version));

    if (activeVersion() == version) {
        return Result<void>::fail(ErrorCode::RuntimeUnsupported,
                                  QStringLiteral("사용 중인 런타임은 제거할 수 없습니다. 다른 버전을 활성화하세요"));
    }

    if (!QDir(runtime->directory).removeRecursively())
        return Result<void>::fail(ErrorCode::DiskError,
                                  QStringLiteral("런타임 디렉터리를 삭제할 수 없습니다: %1").arg(runtime->directory));

    scan();
    return Result<void>::ok();
}

Result<InstalledRuntime> RuntimeManager::registerFromDirectory(const QString& sourceDirectory)
{
    auto document = json::readObjectOrEmpty(
        QDir(sourceDirectory).filePath(RuntimeManager::runtimeManifestName()));
    if (!document)
        return Result<InstalledRuntime>::fail(ErrorCode::RuntimeMissing,
                                              QStringLiteral("runtime.json 을 읽을 수 없습니다: %1").arg(sourceDirectory));

    const QJsonObject object = document.value();
    if (object.value(QStringLiteral("format")).toString() != QStringLiteral("NAPP-RUNTIME")) {
        return Result<InstalledRuntime>::fail(ErrorCode::RuntimeUnsupported,
                                              QStringLiteral("NAPP-RUNTIME 문서가 아닙니다"));
    }

    const QString version = object.value(QStringLiteral("version")).toString();
    if (!SemVer::isValid(version))
        return Result<InstalledRuntime>::fail(ErrorCode::RuntimeUnsupported,
                                              QStringLiteral("런타임 버전이 semver가 아닙니다: %1").arg(version));

    const QString platform = object.value(QStringLiteral("platform")).toString();
    if (!platform.isEmpty() && platform != currentPlatformId()) {
        return Result<InstalledRuntime>::fail(
            ErrorCode::RuntimeUnsupported,
            QStringLiteral("현재 플랫폼(%1)용 런타임이 아닙니다").arg(currentPlatformId()));
    }

    const QString host = object.value(QStringLiteral("host")).toString(RuntimeManager::hostExecutableName());
    if (!QFileInfo::exists(QDir(sourceDirectory).filePath(host))) {
        return Result<InstalledRuntime>::fail(ErrorCode::RuntimeMissing,
                                              QStringLiteral("런타임 실행 파일이 없습니다: %1").arg(host));
    }

    const QString destination = m_paths.runtimeVersionDir(version);
    if (QFileInfo::exists(destination)) {
        return Result<InstalledRuntime>::fail(ErrorCode::RuntimeUnsupported,
                                              QStringLiteral("이미 설치된 런타임 버전입니다: %1").arg(version));
    }

    if (!copyRecursively(sourceDirectory, destination, false)) {
        QDir(destination).removeRecursively();
        return Result<InstalledRuntime>::fail(ErrorCode::DiskError,
                                              QStringLiteral("런타임을 복사할 수 없습니다: %1").arg(destination));
    }

    scan();
    const InstalledRuntime* runtime = find(version);
    if (!runtime || !runtime->valid) {
        return Result<InstalledRuntime>::fail(ErrorCode::RuntimeUnsupported,
                                              runtime ? runtime->problem : QStringLiteral("런타임 등록 실패"));
    }

    if (activeVersion().isEmpty())
        activate(version);

    qInfo("runtime registered: %s (%s)", qPrintable(version), qPrintable(destination));
    return *runtime;
}

bool RuntimeManager::supports(const QString& minimumVersion) const
{
    const InstalledRuntime* runtime = active();
    if (!runtime)
        return false;
    if (minimumVersion.isEmpty())
        return true;
    const auto comparison = SemVer::compareStrings(runtime->version, minimumVersion);
    return comparison.has_value() && comparison.value() >= 0;
}

} // namespace napp::core
