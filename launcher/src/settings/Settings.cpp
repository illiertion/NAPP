#include "Settings.h"

#include "napp/core/json.h"

#include <QJsonObject>
#include <QLocale>

namespace napp::settings {

QString themeToString(ThemeMode mode)
{
    switch (mode) {
    case ThemeMode::Light: return QStringLiteral("light");
    case ThemeMode::Dark: return QStringLiteral("dark");
    case ThemeMode::System: break;
    }
    return QStringLiteral("system");
}

ThemeMode themeFromString(const QString& value)
{
    if (value == QStringLiteral("light"))
        return ThemeMode::Light;
    if (value == QStringLiteral("dark"))
        return ThemeMode::Dark;
    return ThemeMode::System;
}

Settings::Settings(Paths paths, QObject* parent)
    : QObject(parent)
    , m_paths(std::move(paths))
{
}

Result<void> Settings::load()
{
    auto document = json::readObjectOrEmpty(m_paths.settingsFile());
    if (!document)
        return document.error();

    const QJsonObject object = document.value();
    m_theme = themeFromString(object.value(QStringLiteral("theme")).toString(QStringLiteral("system")));
    m_checkUpdatesOnStartup = object.value(QStringLiteral("check_updates_on_startup")).toBool(true);
    // First run: follow the OS language instead of always starting in English.
    // 첫 실행: 항상 영어로 시작하지 않고 OS 언어를 따른다
    const QString storedLanguage = object.value(QStringLiteral("language")).toString();
    m_languageDefaulted = storedLanguage.isEmpty();
    if (m_languageDefaulted) {
        m_language = QLocale::system().language() == QLocale::Korean ? QStringLiteral("ko")
                                                                      : QStringLiteral("en");
    } else {
        m_language = storedLanguage.startsWith(QStringLiteral("ko")) ? QStringLiteral("ko")
                                                                      : QStringLiteral("en");
    }
    m_accent = object.value(QStringLiteral("accent")).toString(QStringLiteral("blue"));
    m_associateIpkg = object.value(QStringLiteral("associate_ipkg")).toBool(true);
    m_windowGeometry = object.value(QStringLiteral("window_geometry")).toString();
    return Result<void>::ok();
}

Result<void> Settings::save() const
{
    QJsonObject object;
    object.insert(QStringLiteral("schema_version"), 1);
    object.insert(QStringLiteral("theme"), themeToString(m_theme));
    object.insert(QStringLiteral("check_updates_on_startup"), m_checkUpdatesOnStartup);
    object.insert(QStringLiteral("language"), m_language);
    object.insert(QStringLiteral("accent"), m_accent);
    object.insert(QStringLiteral("associate_ipkg"), m_associateIpkg);
    object.insert(QStringLiteral("window_geometry"), m_windowGeometry);
    return json::writeObjectAtomically(m_paths.settingsFile(), object);
}

void Settings::setTheme(ThemeMode theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    save();
    emit changed();
}

void Settings::setCheckUpdatesOnStartup(bool enabled)
{
    if (m_checkUpdatesOnStartup == enabled)
        return;
    m_checkUpdatesOnStartup = enabled;
    save();
    emit changed();
}

void Settings::setLanguage(const QString& language)
{
    if (m_language == language)
        return;
    m_language = language;
    save();
    emit changed();
}

void Settings::setAccent(const QString& accent)
{
    if (m_accent == accent)
        return;
    m_accent = accent;
    save();
    emit changed();
}

void Settings::setAssociateIpkg(bool enabled)
{
    if (m_associateIpkg == enabled)
        return;
    m_associateIpkg = enabled;
    save();
    emit changed();
}

void Settings::setWindowGeometry(const QString& base64)
{
    if (m_windowGeometry == base64)
        return;
    m_windowGeometry = base64;
    save(); // no changed(): geometry is not a UI-visible setting // UI에 보이는 설정이 아니므로 changed() 없음
}

} // namespace napp::settings
