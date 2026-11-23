import QtQuick
import "."

// Placeholder cover art: a deterministic two-tone gradient with the app's
// initial. Store icons are not downloaded yet (docs/USAGE.md §11).
Rectangle {
    id: cover

    property string title: ""
    property int seed: 0
    // User-picked icon (config/icons/<appId>.png). Empty keeps the generated
    // gradient cover, so every tile still has artwork without a store icon.
    // 사용자가 고른 아이콘. 비어 있으면 그라데이션 기본 커버를 그린다.
    property string source: ""

    radius: 10
    clip: true
    color: cover.bottomTone

    readonly property var palettes: [
        ["#274072", "#0c1220"],
        ["#33415c", "#0d1117"],
        ["#452f5e", "#130e1d"],
        ["#1d4650", "#0a1518"],
        ["#4d342a", "#170f0c"],
        ["#24422f", "#0b1710"]
    ]
    readonly property var pair: palettes[Math.abs(seed) % palettes.length]
    readonly property color topTone: pair[0]
    readonly property color bottomTone: pair[1]

    gradient: Gradient {
        GradientStop { position: 0.0; color: cover.topTone }
        GradientStop { position: 1.0; color: cover.bottomTone }
    }

    // faint wireframe diagonal, echoing the launcher mark
    Rectangle {
        width: parent.width * 1.6
        height: 1
        rotation: -28
        x: -parent.width * 0.3
        y: parent.height * 0.72
        color: "#1affffff"
    }

    Image {
        anchors.fill: parent
        visible: cover.source.length > 0
        source: cover.source
        sourceSize.width: 256
        sourceSize.height: 256
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        smooth: true
    }

    Text {
        anchors.centerIn: parent
        visible: cover.source.length === 0
        text: cover.title.length > 0 ? cover.title.charAt(0).toUpperCase() : "N"
        color: "#ffffff"
        opacity: 0.9
        font.pixelSize: Math.max(14, Math.min(cover.width, cover.height) * 0.36)
        font.bold: true
    }

    Rectangle {
        anchors.fill: parent
        radius: cover.radius
        color: "transparent"
        border.width: 1
        border.color: "#1affffff"
    }
}
