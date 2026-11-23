import QtQuick
import QtQuick.Controls
import "."

TextField {
    id: control

    implicitHeight: 34
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: 13
    color: Theme.text
    placeholderTextColor: Theme.muted
    selectionColor: Theme.accent
    selectedTextColor: "#ffffff"

    background: Rectangle {
        radius: 7
        color: Theme.surfaceAlt
        border.width: 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
