import QtQuick
import QtQuick.Layouts
import "."

RowLayout {
    id: header

    property string title: ""
    property string note: ""

    spacing: 10

    Text {
        text: header.title
        color: Theme.text
        font.pixelSize: 15
        font.bold: true
    }

    Text {
        text: header.note
        color: Theme.muted
        font.pixelSize: 12
        visible: header.note.length > 0
    }

    Item { Layout.fillWidth: true }
}
