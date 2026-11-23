pragma Singleton

import QtQuick

// Single palette source for every screen and control. Main.qml flips `dark`
// and `accentId` from LauncherBackend, so QML and the native dialogs agree.
QtObject {
    property bool dark: true
    property string accentId: "blue"

    readonly property color bg: dark ? "#080a0e" : "#f4f6fa"
    readonly property color bgElevated: dark ? "#0c0e13" : "#ffffff"
    readonly property color surface: dark ? "#12151b" : "#ffffff"
    readonly property color surfaceAlt: dark ? "#181c24" : "#eef1f7"
    readonly property color hover: dark ? "#1e232d" : "#e5eaf4"
    readonly property color chip: dark ? "#141a25" : "#e8eefa"
    readonly property color border: dark ? "#222833" : "#dde3ed"
    readonly property color borderStrong: dark ? "#333b49" : "#c2ccdd"
    readonly property color text: dark ? "#f3f6fb" : "#0b0e13"
    readonly property color muted: dark ? "#7f8999" : "#5b6472"
    readonly property color mutedLight: dark ? "#b6bfce" : "#3b4350"

    readonly property color accent: accentId === "violet" ? "#8b5cf6"
                                   : accentId === "mint" ? "#1fc4a4"
                                   : accentId === "amber" ? "#e9a23b"
                                   : "#3d7df0"
    readonly property color accentSoft: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.16 : 0.12)
    readonly property color accentLine: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.55 : 0.45)
    readonly property color navActive: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.13 : 0.10)

    readonly property color danger: dark ? "#e8615a" : "#c4342b"
    readonly property color warning: dark ? "#e8ac46" : "#a9700a"
    readonly property color success: dark ? "#3ecf8e" : "#128a5b"

    readonly property color heroTop: dark ? "#16233c" : "#e3ecfd"
    readonly property color heroBottom: dark ? "#0a0c11" : "#f8fafe"
    readonly property color heroText: dark ? "#ffffff" : "#0a1730"
    readonly property color heroSub: dark ? "#a8b6cf" : "#41506b"

    function setDark(value) {
        dark = value
    }

    function setAccent(value) {
        accentId = value
    }
}
