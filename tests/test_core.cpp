#include "napp/core/json.h"
#include "napp/core/paths.h"
#include "napp/core/platform.h"
#include "napp/core/semver.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace napp;

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void semverParsesAndCompares();
    void semverRejectsInvalid();
    void semverPrereleaseOrdering();
    void pathsContainment();
    void pathsResolveInside();
    void jsonRejectsNonObject();
};

void TestCore::semverParsesAndCompares()
{
    const auto left = SemVer::parse(QStringLiteral("1.10.0"));
    const auto right = SemVer::parse(QStringLiteral("1.9.3"));
    QVERIFY(left.has_value());
    QVERIFY(right.has_value());
    QCOMPARE(left->compare(*right), 1);
    QCOMPARE(right->compare(*left), -1);
    QCOMPARE(left->compare(*left), 0);

    const auto buildMeta = SemVer::parse(QStringLiteral("2.0.0+build.7"));
    QVERIFY(buildMeta.has_value());
    QCOMPARE(buildMeta->compare(*SemVer::parse(QStringLiteral("2.0.0"))), 0);
    QCOMPARE(buildMeta->toString(), QStringLiteral("2.0.0+build.7"));
}

void TestCore::semverRejectsInvalid()
{
    for (const QString& value : {QStringLiteral("1.0"), QStringLiteral("1.0.0.0"), QStringLiteral("v1.0.0"),
                                 QStringLiteral("01.0.0"), QStringLiteral("1.0.0-"), QString()}) {
        QVERIFY2(!SemVer::isValid(value), qPrintable(value));
    }
    QVERIFY(SemVer::isValid(QStringLiteral("0.0.1")));
    QVERIFY(SemVer::isValid(QStringLiteral("1.2.3-rc.1")));
}

void TestCore::semverPrereleaseOrdering()
{
    // 1.0.0-alpha < 1.0.0-alpha.1 < 1.0.0-alpha.beta < 1.0.0-beta < 1.0.0
    const QStringList ordered{
        QStringLiteral("1.0.0-alpha"),       QStringLiteral("1.0.0-alpha.1"),
        QStringLiteral("1.0.0-alpha.beta"),  QStringLiteral("1.0.0-beta"),
        QStringLiteral("1.0.0"),
    };
    for (int index = 0; index + 1 < ordered.size(); ++index) {
        const auto result = SemVer::compareStrings(ordered.at(index), ordered.at(index + 1));
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), -1);
    }
}

void TestCore::pathsContainment()
{
    const QString base = QStringLiteral("/tmp/napp/apps/com.example");
    QVERIFY(Paths::isInside(base, base + QStringLiteral("/1.0.0/index.html")));
    QVERIFY(Paths::isInside(base, base));
    QVERIFY(!Paths::isInside(base, base + QStringLiteral("/../other/index.html")));
    QVERIFY(!Paths::isInside(base, QStringLiteral("/tmp/napp/apps/com.example2/index.html")));
}

void TestCore::pathsResolveInside()
{
    const QString base = QStringLiteral("/tmp/napp/apps/com.example/1.0.0");
    QCOMPARE(Paths::resolveInside(base, QStringLiteral("app/index.html")),
             base + QStringLiteral("/app/index.html"));
    QVERIFY(Paths::resolveInside(base, QStringLiteral("../escape.txt")).isEmpty());
    QVERIFY(Paths::resolveInside(base, QStringLiteral("/etc/passwd")).isEmpty());
    QVERIFY(Paths::resolveInside(base, QStringLiteral("app\\..\\x")).isEmpty());
}

void TestCore::jsonRejectsNonObject()
{
    auto array = json::parseObject(QByteArrayLiteral("[1,2,3]"), ErrorCode::ManifestSyntax, QStringLiteral("test"));
    QVERIFY(array.isError());
    QCOMPARE(array.error().code, ErrorCode::ManifestSyntax);

    auto broken = json::parseObject(QByteArrayLiteral("{ broken"), ErrorCode::ManifestSyntax, QStringLiteral("test"));
    QVERIFY(broken.isError());

    auto good = json::parseObject(QByteArrayLiteral("{\"a\":1}"), ErrorCode::ManifestSyntax, QStringLiteral("test"));
    QVERIFY(good.isOk());
    QCOMPARE(good.value().value(QStringLiteral("a")).toInt(), 1);
}

QTEST_MAIN(TestCore)
#include "test_core.moc"
