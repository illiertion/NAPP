import QtQuick
import QtQuick.Controls
import "."

ProgressBar {
    id: control

    implicitHeight: 4
    from: 0
    to: 1

    background: Rectangle {
        implicitHeight: 4
        radius: 2
        color: Theme.surfaceAlt
    }

    contentItem: Item {
        implicitHeight: 4

        Rectangle {
            width: Math.max(0, Math.min(1, control.visualPosition)) * parent.width
            height: parent.height
            radius: 2
            color: Theme.accent
        }
    }
}
