import QtQuick
import QtQuick.Controls
import "."

Button {
    id: control

    // primary | secondary | ghost | danger
    property string variant: "secondary"
    property bool small: false
    property string tooltip: ""

    implicitHeight: small ? 30 : 36
    implicitWidth: Math.max(small ? 40 : 92, contentItem.implicitWidth + (small ? 26 : 36))
    padding: 0
    font.pixelSize: small ? 12 : 13
    font.weight: Font.Medium

    ToolTip.visible: control.hovered && control.tooltip.length > 0
    ToolTip.text: control.tooltip
    ToolTip.delay: 550

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.muted
             : control.variant === "primary" ? "#ffffff"
             : control.variant === "danger" ? Theme.danger
             : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 8
        color: {
            if (!control.enabled)
                return Theme.surfaceAlt
            if (control.variant === "primary") {
                if (control.pressed)
                    return Qt.darker(Theme.accent, 1.15)
                return control.hovered ? Qt.lighter(Theme.accent, 1.12) : Theme.accent
            }
            if (control.variant === "ghost") {
                return control.hovered ? Theme.hover : "transparent"
            }
            if (control.variant === "danger")
                return control.pressed ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.22)
                     : (control.hovered ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.14)
                                        : "transparent")
            return control.pressed ? Theme.border
                 : (control.hovered ? Theme.hover : Theme.surfaceAlt)
        }
        border.width: control.variant === "primary" ? 0 : 1
        border.color: !control.enabled
                      ? Theme.border
                      : control.variant === "danger"
                        ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.55)
                        : control.variant === "ghost"
                          ? "transparent"
                          : (control.activeFocus ? Theme.accent : Theme.border)

        Behavior on color {
            ColorAnimation { duration: 110 }
        }
    }
}
