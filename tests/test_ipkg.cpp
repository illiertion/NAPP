#include "napp/core/paths.h"
#include "napp/core/platform.h"
#include "napp/ipkg/installer.h"
#include "napp/ipkg/manifest_parser.h"
#include "napp/ipkg/package_verifier.h"
#include "napp/ipkg/path_rules.h"
#include "napp/ipkg/zip_reader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace napp;
using namespace napp::ipkg;

namespace {

QString fixture(const QString& name)
{
    return QDir(QStringLiteral(NAPP_FIXTURE_DIR)).filePath(name);
}

QByteArray validManifest(const QString& appId = QStringLiteral("com.illiertion.fixture"))
{
    return QStringLiteral(R"({
        "format": "IPKG",
        "format_version": 1,
        "app": {
            "id": "%1",
            "name": "Fixture App",
            "version": "1.0.0",
            "description": "Test fixture",
            "author": "ILLIERTION",
            "license": "MIT"
        },
        "runtime": { "type": "chromium", "minimum_version": "1.0.0" },
        "sdk": { "minimum_version": "1.0.0" },
        "entry": "app/index.html",
        "platforms": ["windows-x64", "linux-x64"],
        "permissions": ["storage"],
        "metadata": { "icon": "metadata/icon.png", "preview": "metadata/preview.png" }
    })")
        .arg(appId)
        .toUtf8();
}

} // namespace

class TestIpkg : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void manifestParsesExample();
    void manifestRejectsUppercaseId();
    void manifestRejectsUnknownPermission();
    void manifestRejectsUnknownTopLevelKey();
    void manifestRejectsEntryOutsideApp();
    void manifestRejectsNonHttpsHomepage();
    void manifestParsesWindowBlock();
    void manifestRejectsWindowWithoutPermission();
    void manifestRejectsUnknownWindowKey();
    void manifestRejectsUnsafeWindowIcon();

    void pathRulesRejectUnsafeNames_data();
    void pathRulesRejectUnsafeNames();
    void pathRulesAcceptNormalNames();

    void validPackageVerifies();
    void hashMismatchIsDetected();
    void traversalPackageRejected();
    void absolutePathRejected();
    void duplicateEntryRejected();
    void symlinkRejected();
    void encryptedEntryRejected();
    void compressionBombRejected();
    void missingManifestRejected();
    void brokenManifestRejected();
    void brokenIdRejected();
    void missingEntryFileRejected();
    void windowIconIsInstalled();
    void missingWindowIconRejected();

    void installsAndActivates();
    void installIsAtomicForExistingVersion();
    void platformMismatchBlocksInstall();
    void updateKeepsDataAndSwitchesPointer();
    void removeDeletesProgramAndOptionallyData();

private:
    QTemporaryDir m_root;
};

void TestIpkg::initTestCase()
{
    QVERIFY(m_root.isValid());
    QVERIFY2(QFileInfo::exists(fixture(QStringLiteral("valid.ipkg"))),
             "tests/fixtures/*.ipkg 가 없습니다. python tests/fixtures/make_fixtures.py 실행 필요");
}

void TestIpkg::manifestParsesExample()
{
    const auto manifest = ManifestParser::parse(validManifest());
    QVERIFY2(manifest.isOk(), qPrintable(manifest.error().message));
    QCOMPARE(manifest.value().appId, QStringLiteral("com.illiertion.fixture"));
    QCOMPARE(manifest.value().version, QStringLiteral("1.0.0"));
    QCOMPARE(manifest.value().entry, QStringLiteral("app/index.html"));
    QCOMPARE(manifest.value().permissions, QStringList{QStringLiteral("storage")});
    QCOMPARE(manifest.value().minimumRuntimeVersion, QStringLiteral("1.0.0"));
    QCOMPARE(manifest.value().minimumSdkVersion, QStringLiteral("1.0.0"));
}

void TestIpkg::manifestRejectsUppercaseId()
{
    QByteArray bytes = validManifest();
    bytes.replace("\"com.illiertion.fixture\"", "\"Com.Illiertion.Fixture\"");
    const auto manifest = ManifestParser::parse(bytes);
    QVERIFY(manifest.isError());
    QCOMPARE(manifest.error().code, ErrorCode::ManifestSchema);
    QCOMPARE(specCode(manifest.error().code), QStringLiteral("E1005"));
}

void TestIpkg::manifestRejectsUnknownPermission()
{
    QByteArray bytes = validManifest();
    bytes.replace("\"permissions\": [\"storage\"]", "\"permissions\": [\"root-access\"]");
    const auto manifest = ManifestParser::parse(bytes);
    QVERIFY(manifest.isError());
    QCOMPARE(specCode(manifest.error().code), QStringLiteral("E1020"));
}

void TestIpkg::manifestRejectsUnknownTopLevelKey()
{
    QByteArray bytes = validManifest();
    bytes.replace("\"format\": \"IPKG\",", "\"format\": \"IPKG\", \"surprise\": true,");
    const auto manifest = ManifestParser::parse(bytes);
    QVERIFY(manifest.isError());
    QCOMPARE(manifest.error().code, ErrorCode::ManifestSchema);
}

void TestIpkg::manifestRejectsEntryOutsideApp()
{
    QByteArray bytes = validManifest();
    bytes.replace("\"entry\": \"app/index.html\"", "\"entry\": \"index.html\"");
    const auto manifest = ManifestParser::parse(bytes);
    QVERIFY(manifest.isError());
    QCOMPARE(manifest.error().code, ErrorCode::ManifestSchema);
}

void TestIpkg::manifestRejectsNonHttpsHomepage()
{
    QByteArray bytes = validManifest();
    bytes.replace("\"license\": \"MIT\"", "\"license\": \"MIT\", \"homepage\": \"http://insecure.example\"");
    const auto manifest = ManifestParser::parse(bytes);
    QVERIFY(manifest.isError());
    QCOMPARE(manifest.error().code, ErrorCode::ManifestSchema);
}

// window.icon is the image the runtime host draws for the window, the taskbar
// button and the tray. It is optional, but when present it must be a plain
// relative path inside the package.
// window.icon 는 런타임 호스트가 창/작업 표시줄/트레이에 그릴 이미지다. 선택
// 사항이지만, 있으면 패키지 안의 상대 경로여야 한다.
void TestIpkg::manifestParsesWindowBlock()
{
    QByteArray bytes = validManifest();
    bytes.replace("\"permissions\": [\"storage\"]",
                  "\"permissions\": [\"storage\", \"window\"], "
                  "\"window\": { \"title\": \"Fixture Window\", \"width\": 1024, "
                  "\"height\": 720, \"icon\": \"metadata/window-icon.png\", \"tray\": true }");

    const auto manifest = ManifestParser::parse(bytes);
    QVERIFY2(manifest.isOk(), qPrintable(manifest.error().message));
    QCOMPARE(manifest.value().windowTitle, QStringLiteral("Fixture Window"));
    QCOMPARE(manifest.value().windowWidth, 1024);
    QCOMPARE(manifest.value().windowHeight, 720);
    QCOMPARE(manifest.value().windowIconPath, QStringLiteral("metadata/window-icon.png"));
    QVERIFY(manifest.value().windowTray);
}

void TestIpkg::manifestRejectsWindowWithoutPermission()
{
    QByteArray bytes = validManifest();
    bytes.replace("\"permissions\": [\"storage\"]",
                  "\"permissions\": [\"storage\"], \"window\": { \"title\": \"Fixture Window\" }");
    const auto manifest = ManifestParser::parse(bytes);
    QVERIFY(manifest.isError());
    QCOMPARE(manifest.error().code, ErrorCode::ManifestSchema);
}

void TestIpkg::manifestRejectsUnknownWindowKey()
{
    QByteArray bytes = validManifest();
    bytes.replace("\"permissions\": [\"storage\"]",
                  "\"permissions\": [\"storage\", \"window\"], "
                  "\"window\": { \"icon\": \"metadata/icon.png\", \"opacity\": 0.5 }");
    const auto manifest = ManifestParser::parse(bytes);
    QVERIFY(manifest.isError());
    QCOMPARE(manifest.error().code, ErrorCode::ManifestSchema);
}

void TestIpkg::manifestRejectsUnsafeWindowIcon()
{
    QByteArray bytes = validManifest();
    bytes.replace("\"permissions\": [\"storage\"]",
                  "\"permissions\": [\"storage\", \"window\"], "
                  "\"window\": { \"icon\": \"../outside.png\" }");
    const auto manifest = ManifestParser::parse(bytes);
    QVERIFY(manifest.isError());
    QCOMPARE(manifest.error().code, ErrorCode::ManifestSchema);
}

void TestIpkg::pathRulesRejectUnsafeNames_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("isDirectory");

    QTest::newRow("parent") << QStringLiteral("../escape.txt") << false;
    QTest::newRow("nested parent") << QStringLiteral("app/../../escape.txt") << false;
    QTest::newRow("absolute") << QStringLiteral("/etc/passwd") << false;
    QTest::newRow("drive") << QStringLiteral("C:/Windows/system32") << false;
    QTest::newRow("backslash") << QStringLiteral("app\\index.html") << false;
    QTest::newRow("empty segment") << QStringLiteral("app//index.html") << false;
    QTest::newRow("dot") << QStringLiteral("app/./index.html") << false;
    QTest::newRow("reserved") << QStringLiteral("app/CON") << false;
    QTest::newRow("trailing dot") << QStringLiteral("app/file.") << false;
    QTest::newRow("trailing space") << QStringLiteral("app/file ") << false;
    QTest::newRow("forbidden char") << QStringLiteral("app/a:b") << false;
    QTest::newRow("file with slash") << QStringLiteral("app/dir/") << false;
}

void TestIpkg::pathRulesRejectUnsafeNames()
{
    QFETCH(QString, name);
    QFETCH(bool, isDirectory);
    const auto result = paths::validateEntryName(name, isDirectory);
    QVERIFY2(result.isError(), qPrintable(name));
    QCOMPARE(result.error().code, ErrorCode::UnsafePath);
}

void TestIpkg::pathRulesAcceptNormalNames()
{
    QVERIFY(paths::validateEntryName(QStringLiteral("app/index.html"), false).isOk());
    QVERIFY(paths::validateEntryName(QStringLiteral("resources/fonts/한글.ttf"), false).isOk());
    QVERIFY(paths::validateEntryName(QStringLiteral("app/"), true).isOk());
    QVERIFY(paths::validateEntryName(QStringLiteral("metadata/icon.png"), false).isOk());
}

void TestIpkg::validPackageVerifies()
{
    PackageVerifier verifier;
    const auto package = verifier.verify(fixture(QStringLiteral("valid.ipkg")));
    QVERIFY2(package.isOk(), qPrintable(package.error().message));
    QCOMPARE(package.value().manifest.appId, QStringLiteral("com.illiertion.fixture"));
    QCOMPARE(package.value().sha256.size(), 64);
    QVERIFY(package.value().entries.size() >= 5);
}

void TestIpkg::hashMismatchIsDetected()
{
    PackageVerifier verifier;
    const auto package = verifier.verify(fixture(QStringLiteral("valid.ipkg")), QString(64, QLatin1Char('a')));
    QVERIFY(package.isError());
    QCOMPARE(package.error().code, ErrorCode::HashMismatch);
    QCOMPARE(specCode(package.error().code), QStringLiteral("E1006"));
}

void TestIpkg::traversalPackageRejected()
{
    PackageVerifier verifier;
    const auto package = verifier.verify(fixture(QStringLiteral("traversal.ipkg")));
    QVERIFY(package.isError());
    QCOMPARE(package.error().code, ErrorCode::UnsafePath);
    QCOMPARE(specCode(package.error().code), QStringLiteral("E1010"));
}

void TestIpkg::absolutePathRejected()
{
    PackageVerifier verifier;
    QCOMPARE(verifier.verify(fixture(QStringLiteral("absolute.ipkg"))).error().code, ErrorCode::UnsafePath);
}

void TestIpkg::duplicateEntryRejected()
{
    PackageVerifier verifier;
    QCOMPARE(verifier.verify(fixture(QStringLiteral("duplicate.ipkg"))).error().code, ErrorCode::DuplicatePath);
}

void TestIpkg::symlinkRejected()
{
    PackageVerifier verifier;
    QCOMPARE(verifier.verify(fixture(QStringLiteral("symlink.ipkg"))).error().code, ErrorCode::SpecialFile);
}

void TestIpkg::encryptedEntryRejected()
{
    PackageVerifier verifier;
    QCOMPARE(verifier.verify(fixture(QStringLiteral("encrypted.ipkg"))).error().code, ErrorCode::EncryptedEntry);
}

void TestIpkg::compressionBombRejected()
{
    PackageVerifier verifier;
    QCOMPARE(verifier.verify(fixture(QStringLiteral("bomb.ipkg"))).error().code, ErrorCode::CompressionRatio);
}

void TestIpkg::missingManifestRejected()
{
    PackageVerifier verifier;
    const auto package = verifier.verify(fixture(QStringLiteral("nomanifest.ipkg")));
    QCOMPARE(package.error().code, ErrorCode::ManifestMissing);
    QCOMPARE(specCode(package.error().code), QStringLiteral("E1003"));
}

void TestIpkg::brokenManifestRejected()
{
    PackageVerifier verifier;
    QCOMPARE(verifier.verify(fixture(QStringLiteral("badmanifest.ipkg"))).error().code, ErrorCode::ManifestSyntax);
}

void TestIpkg::brokenIdRejected()
{
    PackageVerifier verifier;
    QCOMPARE(verifier.verify(fixture(QStringLiteral("badid.ipkg"))).error().code, ErrorCode::ManifestSchema);
}

void TestIpkg::missingEntryFileRejected()
{
    PackageVerifier verifier;
    QCOMPARE(verifier.verify(fixture(QStringLiteral("noentry.ipkg"))).error().code, ErrorCode::EntryFileMissing);
}

void TestIpkg::windowIconIsInstalled()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    Installer installer(Paths::forRoot(QDir(root.path()).filePath(QStringLiteral("install"))));

    InstallRequest request;
    request.packagePath = fixture(QStringLiteral("window.ipkg"));
    auto installed = installer.install(request);
    QVERIFY2(installed.isOk(), qPrintable(installed.error().message));

    QCOMPARE(installed.value().windowTitle, QStringLiteral("Fixture Window"));
    QCOMPARE(installed.value().windowWidth, 1024);
    QCOMPARE(installed.value().windowHeight, 720);
    QVERIFY(installed.value().windowTray);
    // The icon is surfaced as a path relative to the app root so the launcher can
    // resolve it to an absolute path before handing it to the runtime host.
    // 아이콘은 앱 루트 기준 상대 경로로 전달되고, 런처가 절대 경로로 바꿔 호스트에 넘긴다.
    QCOMPARE(installed.value().windowIconRelPath, QStringLiteral("metadata/window-icon.png"));
    QVERIFY(QFileInfo::exists(
        QDir(installed.value().directory).filePath(installed.value().windowIconRelPath)));
}

void TestIpkg::missingWindowIconRejected()
{
    PackageVerifier verifier;
    QCOMPARE(verifier.verify(fixture(QStringLiteral("window-missing-icon.ipkg"))).error().code,
             ErrorCode::MetadataFileMissing);
}

void TestIpkg::installsAndActivates()
{
    Installer installer(Paths::forRoot(QDir(m_root.path()).filePath(QStringLiteral("install"))));
    InstallRequest request;
    request.packagePath = fixture(QStringLiteral("valid.ipkg"));

    const auto installed = installer.install(request);
    QVERIFY2(installed.isOk(), qPrintable(installed.error().message));
    QCOMPARE(installed.value().appId, QStringLiteral("com.illiertion.fixture"));
    QCOMPARE(installed.value().version, QStringLiteral("1.0.0"));
    QVERIFY(QFileInfo::exists(QDir(installed.value().directory).filePath(QStringLiteral("app/index.html"))));
    QVERIFY(QFileInfo::exists(QDir(installed.value().directory).filePath(QStringLiteral("napp-package.json"))));

    const auto active = installer.activeVersion(QStringLiteral("com.illiertion.fixture"));
    QVERIFY(active.isOk());
    QCOMPARE(active.value(), QStringLiteral("1.0.0"));

    const auto ids = installer.installedAppIds();
    QVERIFY(ids.isOk());
    QCOMPARE(ids.value(), QStringList{QStringLiteral("com.illiertion.fixture")});

    // Staging must be empty after a successful install.
    QVERIFY(QDir(installer.paths().stagingDir()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
}

void TestIpkg::installIsAtomicForExistingVersion()
{
    Installer installer(Paths::forRoot(QDir(m_root.path()).filePath(QStringLiteral("atomic"))));
    InstallRequest request;
    request.packagePath = fixture(QStringLiteral("valid.ipkg"));

    QVERIFY(installer.install(request).isOk());
    const auto second = installer.install(request);
    QVERIFY(second.isError());
    QCOMPARE(second.error().code, ErrorCode::AppAlreadyInstalled);

    const auto active = installer.activePackage(QStringLiteral("com.illiertion.fixture"));
    QVERIFY(active.isOk());
    QVERIFY(QFileInfo::exists(QDir(active.value().directory).filePath(QStringLiteral("app/index.html"))));
}

void TestIpkg::platformMismatchBlocksInstall()
{
    Installer installer(Paths::forRoot(QDir(m_root.path()).filePath(QStringLiteral("platform"))));
    InstallRequest request;
    request.packagePath = fixture(QStringLiteral("windows-only.ipkg"));

    const auto installed = installer.install(request);
    if (currentPlatformId().startsWith(QStringLiteral("windows"))) {
        QVERIFY2(installed.isOk(), qPrintable(installed.error().message));
    } else {
        QVERIFY(installed.isError());
        QCOMPARE(installed.error().code, ErrorCode::UnsupportedPlatform);
    }
}

void TestIpkg::updateKeepsDataAndSwitchesPointer()
{
    const Paths paths = Paths::forRoot(QDir(m_root.path()).filePath(QStringLiteral("update")));
    Installer installer(paths);
    const QString appId = QStringLiteral("com.illiertion.fixture");

    InstallRequest first;
    first.packagePath = fixture(QStringLiteral("valid.ipkg"));
    const auto initial = installer.install(first);
    QVERIFY2(initial.isOk(), qPrintable(initial.error().message));

    const QString dataFile = QDir(paths.dataDir(appId)).filePath(QStringLiteral("state.json"));
    QDir().mkpath(QFileInfo(dataFile).absolutePath());
    QFile data(dataFile);
    QVERIFY(data.open(QIODevice::WriteOnly));
    data.write("{\"score\":42}");
    data.close();

    InstallRequest second;
    second.packagePath = fixture(QStringLiteral("valid-1.1.0.ipkg"));
    const auto updated = installer.install(second);
    QVERIFY2(updated.isOk(), qPrintable(updated.error().message));

    const auto active = installer.activeVersion(appId);
    QVERIFY(active.isOk());
    QCOMPARE(active.value(), QStringLiteral("1.1.0"));

    // App data survives the update, and the old program version can still be
    // selected explicitly (rollback path).
    QVERIFY(QFileInfo::exists(dataFile));
    QVERIFY(installer.activate(appId, QStringLiteral("1.0.0")).isOk());
    QCOMPARE(installer.activeVersion(appId).value(), QStringLiteral("1.0.0"));

    QVERIFY(installer.pruneVersions(appId, QStringLiteral("1.1.0")).isOk());
    const auto versions = installer.installedVersions(appId);
    QVERIFY(versions.isOk());
    QCOMPARE(versions.value(), QStringList{QStringLiteral("1.1.0")});
}

void TestIpkg::removeDeletesProgramAndOptionallyData()
{
    const Paths paths = Paths::forRoot(QDir(m_root.path()).filePath(QStringLiteral("remove")));
    Installer installer(paths);
    InstallRequest request;
    request.packagePath = fixture(QStringLiteral("valid.ipkg"));
    QVERIFY(installer.install(request).isOk());

    const auto removed = installer.remove(QStringLiteral("com.illiertion.fixture"), true);
    QVERIFY2(removed.isOk(), qPrintable(removed.error().message));
    QVERIFY(!QFileInfo::exists(QDir(paths.appsDir()).filePath(QStringLiteral("com.illiertion.fixture"))));
    QVERIFY(!QFileInfo::exists(paths.dataDir(QStringLiteral("com.illiertion.fixture"))));

    const auto again = installer.remove(QStringLiteral("com.illiertion.fixture"), false);
    QVERIFY(again.isError());
    QCOMPARE(again.error().code, ErrorCode::AppNotFound);
}

QTEST_MAIN(TestIpkg)
#include "test_ipkg.moc"
