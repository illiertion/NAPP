import QtQuick
import QtQuick.Controls
import "."

CheckBox {
    id: control

    spacing: 8
    font.pixelSize: 13

    contentItem: Text {
        text: control.text
        font: control.font
        color: Theme.text
        leftPadding: control.indicator.width + control.spacing
        verticalAlignment: Text.AlignVCenter
    }

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: 4
        color: control.checked ? Theme.accent : Theme.surfaceAlt
        border.width: control.checked ? 0 : 1
        border.color: control.hovered ? Theme.accent : Theme.border

        Text {
            anchors.centerIn: parent
            text: "\u2713"
            visible: control.checked
            color: "#ffffff"
            font.pixelSize: 12
        }
    }
}
