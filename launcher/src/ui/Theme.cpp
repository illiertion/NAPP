#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleHints>

namespace napp::ui {
namespace {

QString styleSheet(ColorScheme scheme, const QString& accent)
{
    const bool dark = scheme == ColorScheme::Dark;
    const QString surface = dark ? QStringLiteral("#1c1f24") : QStringLiteral("#ffffff");
    const QString canvas = dark ? QStringLiteral("#16181d") : QStringLiteral("#f4f5f7");
    const QString border = dark ? QStringLiteral("#2c313a") : QStringLiteral("#dcdfe4");
    const QString text = dark ? QStringLiteral("#e6e8ec") : QStringLiteral("#1a1c20");
    const QString muted = dark ? QStringLiteral("#9aa1ad") : QStringLiteral("#5c626c");
    const QString hover = dark ? QStringLiteral("#232830") : QStringLiteral("#eef1f6");

    return QStringLiteral(R"(
        QWidget { color: %3; }
        QDialog, QMessageBox { background: %1; }
        QLabel { color: %3; }
        QLabel#Muted { color: %4; }
        QLineEdit, QTextEdit, QPlainTextEdit { background: %1; border: 1px solid %2; border-radius: 7px; padding: 6px 9px; color: %3; selection-background-color: %6; }
        QLineEdit:focus { border-color: %6; }
        QPushButton { background: %1; border: 1px solid %2; border-radius: 7px; padding: 6px 14px; color: %3; min-width: 74px; }
        QPushButton:hover { background: %5; border-color: %6; }
        QPushButton:default { background: %6; border-color: %6; color: #ffffff; font-weight: 600; }
        QPushButton:disabled { color: %4; border-color: %2; }
        QProgressBar { border: 1px solid %2; border-radius: 6px; background: %5; text-align: center; color: %3; height: 16px; }
        QProgressBar::chunk { background: %6; border-radius: 5px; }
        QToolTip { background: %1; color: %3; border: 1px solid %2; padding: 4px 7px; }
    )")
        .arg(surface, border, text, muted, hover, accent);
}

QPalette paletteFor(ColorScheme scheme, const QString& accent)
{
    const bool dark = scheme == ColorScheme::Dark;
    QPalette palette;
    if (dark) {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#16181d")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#e6e8ec")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#1c1f24")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#232830")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#e6e8ec")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#1c1f24")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#e6e8ec")));
        palette.setColor(QPalette::Highlight, QColor(accent));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#232830")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#e6e8ec")));
        palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#7b828e")));
    } else {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#f4f5f7")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#1a1c20")));
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#1a1c20")));
        palette.setColor(QPalette::Button, Qt::white);
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#1a1c20")));
        palette.setColor(QPalette::Highlight, QColor(accent));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#8b93a3")));
    }
    return palette;
}

} // namespace

QStringList accentIds()
{
    return {QStringLiteral("blue"), QStringLiteral("violet"), QStringLiteral("mint"),
            QStringLiteral("amber")};
}

QString accentColor(const QString& id)
{
    if (id == QStringLiteral("violet"))
        return QStringLiteral("#8b5cf6");
    if (id == QStringLiteral("mint"))
        return QStringLiteral("#1fc4a4");
    if (id == QStringLiteral("amber"))
        return QStringLiteral("#e9a23b");
    return QStringLiteral("#3d7df0");
}

ColorScheme detectSystemScheme()
{
    const auto hints = QGuiApplication::styleHints();
    return hints->colorScheme() == Qt::ColorScheme::Dark ? ColorScheme::Dark : ColorScheme::Light;
}

void applyTheme(QApplication& application, ColorScheme scheme, const QString& accent, bool pinScheme)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (pinScheme) {
        application.styleHints()->setColorScheme(scheme == ColorScheme::Dark ? Qt::ColorScheme::Dark
                                                                             : Qt::ColorScheme::Light);
    } else {
        // Follow the OS again; without this the previously pinned scheme sticks.
        application.styleHints()->unsetColorScheme();
    }
#else
    Q_UNUSED(pinScheme);
#endif
    const QString resolved = accentColor(accent);
    application.setPalette(paletteFor(scheme, resolved));
    application.setStyleSheet(styleSheet(scheme, resolved));
}

} // namespace napp::ui
