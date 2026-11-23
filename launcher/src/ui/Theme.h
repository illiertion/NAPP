#pragma once

#include <QString>
#include <QStringList>

class QApplication;

namespace napp::ui {

enum class ColorScheme { Light, Dark };

// Accent palette ids offered in Settings. The same ids drive the QML
// Theme singleton (launcher/qml/components/Theme.qml), so keep them in sync.
QStringList accentIds();
QString accentColor(const QString& id);

// Applies a flat, desktop-class palette to the Widgets shell (native dialogs).
// `pinScheme` is false for the "system" theme: pinning would make Qt override
// the OS colour scheme, so later OS theme changes would stop reaching us.
void applyTheme(QApplication& application, ColorScheme scheme, const QString& accent,
                bool pinScheme);
ColorScheme detectSystemScheme();

} // namespace napp::ui
