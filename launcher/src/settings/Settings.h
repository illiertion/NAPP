#pragma once

#include "napp/core/paths.h"
#include "napp/core/result.h"

#include <QObject>
#include <QString>

namespace napp::settings {

enum class ThemeMode { System, Light, Dark };

QString themeToString(ThemeMode mode);
ThemeMode themeFromString(const QString& value);

// Small JSON-backed settings store (config/settings.json).
class Settings : public QObject
{
    Q_OBJECT

public:
    explicit Settings(Paths paths, QObject* parent = nullptr);

    Result<void> load();
    Result<void> save() const;

    ThemeMode theme() const { return m_theme; }
    void setTheme(ThemeMode theme);

    bool checkUpdatesOnStartup() const { return m_checkUpdatesOnStartup; }
    void setCheckUpdatesOnStartup(bool enabled);

    QString language() const { return m_language; }
    void setLanguage(const QString& language);

    // Accent palette id: blue | violet | mint | amber (see ui/Theme.h).
    QString accent() const { return m_accent; }
    void setAccent(const QString& accent);

    // Keep .ipkg files associated with this launcher (double-click installs).
    // .ipkg 파일을 이 런처에 연결해 둔다 (더블클릭하면 설치)
    bool associateIpkg() const { return m_associateIpkg; }
    void setAssociateIpkg(bool enabled);

    // Window geometry restored on the next start (base64 of QWidget::saveGeometry).
    // 다음 실행 때 복원할 창 위치/크기 (QWidget::saveGeometry 의 base64)
    QString windowGeometry() const { return m_windowGeometry; }
    void setWindowGeometry(const QString& base64);

    // True when settings.json had no language yet (first run).
    // settings.json 에 언어가 없었으면(첫 실행) true
    bool languageWasDefaulted() const { return m_languageDefaulted; }

signals:
    void changed();

private:
    Paths m_paths;
    ThemeMode m_theme = ThemeMode::System;
    bool m_checkUpdatesOnStartup = true;
    QString m_language = QStringLiteral("en");
    QString m_accent = QStringLiteral("blue");
    bool m_associateIpkg = true;
    QString m_windowGeometry;
    bool m_languageDefaulted = false;
};

} // namespace napp::settings
