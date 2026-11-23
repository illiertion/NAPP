import QtQuick
import QtQuick.Layouts
import "."

ColumnLayout {
    id: state

    property string glyph: "\u25CB"
    property string title: ""
    property string description: ""

    spacing: 8

    Text {
        text: state.glyph
        color: Theme.muted
        opacity: 0.7
        font.pixelSize: 26
        Layout.alignment: Qt.AlignHCenter
    }

    Text {
        text: state.title
        color: Theme.mutedLight
        font.pixelSize: 13
        font.bold: true
        Layout.alignment: Qt.AlignHCenter
    }

    Text {
        text: state.description
        color: Theme.muted
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        Layout.maximumWidth: 460
        Layout.alignment: Qt.AlignHCenter
        visible: text.length > 0
    }
}
