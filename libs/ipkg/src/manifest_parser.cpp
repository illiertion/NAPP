#include "napp/ipkg/manifest_parser.h"

#include "napp/core/json.h"
#include "napp/core/semver.h"
#include "napp/ipkg/path_rules.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace napp::ipkg {
namespace {

const QStringList kPermissions{
    QStringLiteral("storage"),     QStringLiteral("window"),     QStringLiteral("notifications"),
    QStringLiteral("dialog"),      QStringLiteral("filesystem"), QStringLiteral("clipboard"),
    QStringLiteral("external-links"), QStringLiteral("system-info"),
};

const QSet<QString> kManifestKeys{
    QStringLiteral("format"),         QStringLiteral("format_version"), QStringLiteral("app"),
    QStringLiteral("runtime"),        QStringLiteral("sdk"),            QStringLiteral("entry"),
    QStringLiteral("platforms"),      QStringLiteral("permissions"),    QStringLiteral("metadata"),
    QStringLiteral("window"),         QStringLiteral("extensions"),
};

bool matchesAppId(const QString& value)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[a-z][a-z0-9]*(\\.[a-z0-9][a-z0-9-]*){1,}$"));
    return value.size() <= 128 && pattern.match(value).hasMatch();
}

Result<QString> requireString(const QJsonObject& object, const QString& key, int minLength, int maxLength,
                              const QString& context)
{
    const QJsonValue value = object.value(key);
    if (!value.isString())
        return Result<QString>::fail(ErrorCode::ManifestSchema,
                                     QStringLiteral("%1.%2 필드가 문자열이 아닙니다").arg(context, key));
    const QString text = value.toString();
    if (text.size() < minLength || text.size() > maxLength)
        return Result<QString>::fail(ErrorCode::ManifestSchema,
                                     QStringLiteral("%1.%2 길이가 %3~%4 범위를 벗어났습니다").arg(context, key).arg(minLength).arg(maxLength));
    return text;
}

Result<QString> optionalString(const QJsonObject& object, const QString& key, int maxLength, const QString& context)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined())
        return QString();
    if (!value.isString() || value.toString().size() > maxLength)
        return Result<QString>::fail(ErrorCode::ManifestSchema,
                                     QStringLiteral("%1.%2 형식이 올바르지 않습니다").arg(context, key));
    return value.toString();
}

Result<void> requireHttpsUrl(const QString& value, const QString& context)
{
    if (!value.startsWith(QStringLiteral("https://")))
        return Result<void>::fail(ErrorCode::ManifestSchema,
                                  QStringLiteral("%1 는 https URL이어야 합니다").arg(context));
    return Result<void>::ok();
}

Result<QStringList> requireStringArray(const QJsonObject& object, const QString& key, const QString& context)
{
    const QJsonValue value = object.value(key);
    if (!value.isArray())
        return Result<QStringList>::fail(ErrorCode::ManifestSchema,
                                         QStringLiteral("%1.%2 는 배열이어야 합니다").arg(context, key));
    QStringList items;
    for (const QJsonValue& item : value.toArray()) {
        if (!item.isString())
            return Result<QStringList>::fail(ErrorCode::ManifestSchema,
                                             QStringLiteral("%1.%2 배열 항목이 문자열이 아닙니다").arg(context, key));
        items.append(item.toString());
    }
    return items;
}

} // namespace

QStringList knownPermissions() { return kPermissions; }

QStringList knownPlatforms()
{
    return {
        QStringLiteral("windows-x64"), QStringLiteral("windows-arm64"), QStringLiteral("linux-x64"),
        QStringLiteral("linux-arm64"), QStringLiteral("macos-x64"), QStringLiteral("macos-arm64"),
    };
}

bool isKnownPermission(const QString& permission) { return kPermissions.contains(permission); }

bool isKnownPlatform(const QString& platform) { return knownPlatforms().contains(platform); }

Result<Manifest> ManifestParser::parse(const QByteArray& json, const QString& origin)
{
    auto object = json::parseObject(json, ErrorCode::ManifestSyntax, origin);
    if (!object)
        return object.error();

    const QJsonObject root = object.value();

    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        if (!kManifestKeys.contains(it.key())) {
            return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                          QStringLiteral("알 수 없는 최상위 필드: %1 (확장은 extensions 사용)").arg(it.key()));
        }
    }

    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("IPKG"))
        return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("format 은 \"IPKG\" 여야 합니다"));

    const QJsonValue formatVersion = root.value(QStringLiteral("format_version"));
    if (!formatVersion.isDouble() || formatVersion.toInt() != 1)
        return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                      QStringLiteral("지원하지 않는 format_version 입니다 (v1만 지원)"));

    Manifest manifest;
    manifest.formatVersion = 1;

    const QJsonValue appValue = root.value(QStringLiteral("app"));
    if (!appValue.isObject())
        return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("app 객체가 없습니다"));
    const QJsonObject app = appValue.toObject();

    static const QSet<QString> appKeys{
        QStringLiteral("id"),        QStringLiteral("name"),     QStringLiteral("version"),
        QStringLiteral("description"), QStringLiteral("author"), QStringLiteral("license"),
        QStringLiteral("homepage"),  QStringLiteral("keywords"),
    };
    for (auto it = app.constBegin(); it != app.constEnd(); ++it) {
        if (!appKeys.contains(it.key()))
            return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                          QStringLiteral("알 수 없는 app 필드: %1").arg(it.key()));
    }

    auto id = requireString(app, QStringLiteral("id"), 3, 128, QStringLiteral("app"));
    if (!id)
        return id.error();
    if (!matchesAppId(id.value())) {
        return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                      QStringLiteral("app.id 형식이 올바르지 않습니다(소문자 역DNS): %1").arg(id.value()));
    }
    manifest.appId = id.value();

    auto name = requireString(app, QStringLiteral("name"), 1, 80, QStringLiteral("app"));
    if (!name)
        return name.error();
    manifest.name = name.value();

    auto version = requireString(app, QStringLiteral("version"), 5, 64, QStringLiteral("app"));
    if (!version)
        return version.error();
    if (!SemVer::isValid(version.value()))
        return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                      QStringLiteral("app.version 이 semver가 아닙니다: %1").arg(version.value()));
    manifest.version = version.value();

    auto description = requireString(app, QStringLiteral("description"), 1, 4000, QStringLiteral("app"));
    if (!description)
        return description.error();
    manifest.description = description.value();

    auto author = requireString(app, QStringLiteral("author"), 1, 120, QStringLiteral("app"));
    if (!author)
        return author.error();
    manifest.author = author.value();

    auto license = requireString(app, QStringLiteral("license"), 1, 80, QStringLiteral("app"));
    if (!license)
        return license.error();
    manifest.license = license.value();

    auto homepage = optionalString(app, QStringLiteral("homepage"), 2048, QStringLiteral("app"));
    if (!homepage)
        return homepage.error();
    manifest.homepage = homepage.value();
    if (!manifest.homepage.isEmpty()) {
        auto check = requireHttpsUrl(manifest.homepage, QStringLiteral("app.homepage"));
        if (!check)
            return check.error();
    }

    const QJsonValue keywordsValue = app.value(QStringLiteral("keywords"));
    if (!keywordsValue.isUndefined()) {
        auto keywords = requireStringArray(app, QStringLiteral("keywords"), QStringLiteral("app"));
        if (!keywords)
            return keywords.error();
        if (keywords.value().size() > 12)
            return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("app.keywords 는 최대 12개입니다"));
        for (const QString& keyword : keywords.value()) {
            if (keyword.isEmpty() || keyword.size() > 32)
                return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("app.keywords 항목 길이 오류"));
        }
        manifest.keywords = keywords.value();
    }

    const QJsonValue runtimeValue = root.value(QStringLiteral("runtime"));
    if (!runtimeValue.isObject())
        return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("runtime 객체가 없습니다"));
    const QJsonObject runtime = runtimeValue.toObject();
    for (auto it = runtime.constBegin(); it != runtime.constEnd(); ++it) {
        if (it.key() != QStringLiteral("type") && it.key() != QStringLiteral("minimum_version")) {
            return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                          QStringLiteral("알 수 없는 runtime 필드: %1").arg(it.key()));
        }
    }
    if (runtime.value(QStringLiteral("type")).toString() != QStringLiteral("chromium")) {
        return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("runtime.type 은 \"chromium\" 이어야 합니다"));
    }
    manifest.runtimeType = QStringLiteral("chromium");

    auto minRuntime = requireString(runtime, QStringLiteral("minimum_version"), 5, 64, QStringLiteral("runtime"));
    if (!minRuntime)
        return minRuntime.error();
    if (!SemVer::isValid(minRuntime.value()))
        return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("runtime.minimum_version 이 semver가 아닙니다"));
    manifest.minimumRuntimeVersion = minRuntime.value();

    const QJsonValue sdkValue = root.value(QStringLiteral("sdk"));
    if (!sdkValue.isObject())
        return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("sdk 객체가 없습니다"));
    const QJsonObject sdk = sdkValue.toObject();
    if (sdk.size() != 1 || !sdk.contains(QStringLiteral("minimum_version"))) {
        return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("sdk 필드는 minimum_version 만 허용합니다"));
    }
    auto minSdk = requireString(sdk, QStringLiteral("minimum_version"), 5, 64, QStringLiteral("sdk"));
    if (!minSdk)
        return minSdk.error();
    if (!SemVer::isValid(minSdk.value()))
        return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("sdk.minimum_version 이 semver가 아닙니다"));
    manifest.minimumSdkVersion = minSdk.value();

    auto entry = requireString(root, QStringLiteral("entry"), 3, 512, QStringLiteral("manifest"));
    if (!entry)
        return entry.error();
    manifest.entry = entry.value();
    if (!manifest.entry.startsWith(QStringLiteral("app/"))) {
        return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                      QStringLiteral("entry 는 app/ 하위여야 합니다: %1").arg(manifest.entry));
    }
    auto entryRules = paths::validateEntryName(manifest.entry, false);
    if (!entryRules)
        return entryRules.error();

    auto platforms = requireStringArray(root, QStringLiteral("platforms"), QStringLiteral("manifest"));
    if (!platforms)
        return platforms.error();
    if (platforms.value().isEmpty())
        return Result<Manifest>::fail(ErrorCode::UnsupportedValue, QStringLiteral("platforms 는 최소 1개가 필요합니다"));
    for (const QString& platform : platforms.value()) {
        if (!isKnownPlatform(platform)) {
            return Result<Manifest>::fail(ErrorCode::UnsupportedValue,
                                          QStringLiteral("알 수 없는 플랫폼 식별자: %1").arg(platform));
        }
    }
    manifest.platforms = platforms.value();

    auto permissions = requireStringArray(root, QStringLiteral("permissions"), QStringLiteral("manifest"));
    if (!permissions)
        return permissions.error();
    for (const QString& permission : permissions.value()) {
        if (!isKnownPermission(permission)) {
            return Result<Manifest>::fail(ErrorCode::UnsupportedValue,
                                          QStringLiteral("알 수 없는 권한: %1 (v1 런처는 무시하지 않고 거부합니다)").arg(permission));
        }
    }
    manifest.permissions = permissions.value();

    // Optional window block. It is only meaningful for apps that asked for the
    // "window" permission, so a manifest that uses it without asking is rejected
    // instead of silently changing the window. // window 는 "window" 권한을 요청한
    // 앱에서만 의미가 있으므로, 권한 없이 쓰면 조용히 무시하지 않고 거부한다.
    const QJsonValue windowValue = root.value(QStringLiteral("window"));
    if (!windowValue.isUndefined()) {
        if (!windowValue.isObject())
            return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("window 는 객체여야 합니다"));
        const QJsonObject window = windowValue.toObject();
        for (auto it = window.constBegin(); it != window.constEnd(); ++it) {
            const QString key = it.key();
            if (key != QStringLiteral("title") && key != QStringLiteral("width")
                && key != QStringLiteral("height") && key != QStringLiteral("tray")) {
                return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                              QStringLiteral("알 수 없는 window 필드: %1").arg(key));
            }
        }
        if (!manifest.permissions.contains(QStringLiteral("window"))) {
            return Result<Manifest>::fail(
                ErrorCode::ManifestSchema,
                QStringLiteral("window 를 사용하려면 permissions 에 \"window\" 가 있어야 합니다"));
        }

        if (window.contains(QStringLiteral("title"))) {
            auto title = requireString(window, QStringLiteral("title"), 1, 80, QStringLiteral("window"));
            if (!title)
                return title.error();
            manifest.windowTitle = title.value();
        }

        if (window.contains(QStringLiteral("width"))) {
            const QJsonValue width = window.value(QStringLiteral("width"));
            if (!width.isDouble() || width.toInt() < 480 || width.toInt() > 3840) {
                return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                              QStringLiteral("window.width 는 480~3840 사이의 정수여야 합니다"));
            }
            manifest.windowWidth = width.toInt();
        }

        if (window.contains(QStringLiteral("height"))) {
            const QJsonValue height = window.value(QStringLiteral("height"));
            if (!height.isDouble() || height.toInt() < 360 || height.toInt() > 2160) {
                return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                              QStringLiteral("window.height 는 360~2160 사이의 정수여야 합니다"));
            }
            manifest.windowHeight = height.toInt();
        }

        if (window.contains(QStringLiteral("tray"))) {
            const QJsonValue tray = window.value(QStringLiteral("tray"));
            if (!tray.isBool()) {
                return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                              QStringLiteral("window.tray 는 true 또는 false 여야 합니다"));
            }
            manifest.windowTray = tray.toBool();
        }

    }

    const QJsonValue metadataValue = root.value(QStringLiteral("metadata"));
    if (!metadataValue.isObject())
        return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("metadata 객체가 없습니다"));
    const QJsonObject metadata = metadataValue.toObject();
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
        const QString key = it.key();
        if (key != QStringLiteral("icon") && key != QStringLiteral("preview") && key != QStringLiteral("screenshots")) {
            return Result<Manifest>::fail(ErrorCode::ManifestSchema,
                                          QStringLiteral("알 수 없는 metadata 필드: %1").arg(key));
        }
    }

    auto icon = requireString(metadata, QStringLiteral("icon"), 3, 512, QStringLiteral("metadata"));
    if (!icon)
        return icon.error();
    auto iconRules = paths::validateEntryName(icon.value(), false);
    if (!iconRules)
        return iconRules.error();
    manifest.iconPath = icon.value();

    auto preview = requireString(metadata, QStringLiteral("preview"), 3, 512, QStringLiteral("metadata"));
    if (!preview)
        return preview.error();
    auto previewRules = paths::validateEntryName(preview.value(), false);
    if (!previewRules)
        return previewRules.error();
    manifest.previewPath = preview.value();

    const QJsonValue screenshotsValue = metadata.value(QStringLiteral("screenshots"));
    if (!screenshotsValue.isUndefined()) {
        auto screenshots = requireStringArray(metadata, QStringLiteral("screenshots"), QStringLiteral("metadata"));
        if (!screenshots)
            return screenshots.error();
        if (screenshots.value().size() > 8)
            return Result<Manifest>::fail(ErrorCode::ManifestSchema, QStringLiteral("screenshots 는 최대 8개입니다"));
        for (const QString& screenshot : screenshots.value()) {
            auto rules = paths::validateEntryName(screenshot, false);
            if (!rules)
                return rules.error();
        }
        manifest.screenshots = screenshots.value();
    }

    return manifest;
}

Result<Manifest> ManifestParser::readFrom(const ZipReader& reader)
{
    if (!reader.find(QStringLiteral("manifest.json")))
        return Result<Manifest>::fail(ErrorCode::ManifestMissing, QStringLiteral("패키지 루트에 manifest.json 이 없습니다"));

    auto bytes = reader.read(QStringLiteral("manifest.json"), static_cast<quint64>(kMaxManifestBytes));
    if (!bytes)
        return bytes.error();

    return parse(bytes.value());
}

} // namespace napp::ipkg
