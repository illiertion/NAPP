import QtQuick
import QtQuick.Controls
import "."

ComboBox {
    id: control

    implicitHeight: 34
    implicitWidth: 180
    font.pixelSize: 13
    padding: 0

    contentItem: Text {
        leftPadding: 12
        rightPadding: 26
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.text : Theme.muted
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 7
        color: Theme.surfaceAlt
        border.width: 1
        border.color: control.popup && control.popup.visible ? Theme.accent : Theme.border
    }

    indicator: Text {
        x: control.width - width - 11
        y: control.topPadding + (control.availableHeight - height) / 2
        text: "\u25BE"
        color: Theme.muted
        font.pixelSize: 11
    }

    delegate: ItemDelegate {
        id: entry

        width: control.width
        height: 32
        highlighted: control.highlightedIndex === index

        readonly property string entryLabel: {
            const data = modelData
            if (data === undefined || data === null)
                return ""
            if (typeof data === "string" || typeof data === "number")
                return String(data)
            if (control.textRole && data[control.textRole] !== undefined)
                return String(data[control.textRole])
            return ""
        }

        contentItem: Text {
            text: entry.entryLabel
            color: entry.highlighted ? Theme.text : Theme.mutedLight
            font.pixelSize: 12
            leftPadding: 10
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 6
            color: entry.highlighted ? Theme.hover : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 300)
        padding: 4

        background: Rectangle {
            color: Theme.surfaceAlt
            radius: 8
            border.width: 1
            border.color: Theme.border
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
        }
    }
}
