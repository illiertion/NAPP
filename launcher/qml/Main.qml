import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Rectangle {
    id: root

    readonly property var pageKeys: ["nav.home", "nav.store", "nav.library",
                                     "nav.runtime", "nav.settings", "nav.about"]
    readonly property var pageHints: ["home.subtitle", "store.eyebrow", "library.subtitle",
                                      "runtime.title", "settings.appearance", "about.subtitle"]
    readonly property string pageTitle: I18n.t(pageKeys[backend.currentPage] || "nav.home")
    // Narrow windows collapse the rail to glyphs; every screen keeps working
    // because the content column simply gets the freed width.
    readonly property bool compact: width < 1040

    color: Theme.bg

    Connections {
        target: backend
        function onThemeChanged() {
            Theme.setDark(backend.darkMode)
            Theme.setAccent(backend.accent)
        }
        function onLanguageChanged() {
            I18n.setLanguage(backend.language)
        }
    }

    Component.onCompleted: {
        Theme.setDark(backend.darkMode)
        Theme.setAccent(backend.accent)
        I18n.setLanguage(backend.language)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ------------------------------------------------ navigation rail
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: root.compact ? 74 : 236
            Behavior on Layout.preferredWidth { NumberAnimation { duration: 140 } }
            color: Theme.bgElevated

            Rectangle {
                width: 1
                height: parent.height
                anchors.right: parent.right
                color: Theme.border
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: root.compact ? 6 : 12
                anchors.rightMargin: root.compact ? 6 : 12
                anchors.topMargin: 14
                anchors.bottomMargin: 12
                spacing: 2

                // brand
                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 18
                    spacing: 10

                    Rectangle {
                        width: 38
                        height: 38
                        radius: 10
                        color: "#07080b"
                        border.width: 1
                        border.color: Theme.borderStrong
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: "qrc:/branding/napp-icon-256.png"
                            sourceSize.width: 76
                            sourceSize.height: 76
                            fillMode: Image.PreserveAspectCrop
                            smooth: true
                        }
                    }

                    ColumnLayout {
                        spacing: 0
                        visible: !root.compact

                        Text {
                            text: "NAPP"
                            color: Theme.text
                            font.pixelSize: 18
                            font.bold: true
                            font.letterSpacing: 1.2
                        }
                        Text {
                            text: I18n.t("shell.brandSub")
                            color: Theme.muted
                            font.pixelSize: 9
                            font.letterSpacing: 1.6
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Text {
                    Layout.leftMargin: 12
                    Layout.bottomMargin: 6
                    visible: !root.compact
                    text: I18n.t("shell.workspace")
                    color: Theme.muted
                    font.pixelSize: 9
                    font.letterSpacing: 1.8
                }

                Repeater {
                    model: [
                        { glyph: "\u2302", key: "nav.home" },
                        { glyph: "\u25A6", key: "nav.store" },
                        { glyph: "\u25A4", key: "nav.library" },
                        { glyph: "\u25C8", key: "nav.runtime" },
                        { glyph: "\u2699", key: "nav.settings" },
                        { glyph: "\u24D8", key: "nav.about" }
                    ]

                    delegate: Rectangle {
                        id: navItem

                        required property int index
                        required property var modelData

                        readonly property bool current: backend.currentPage === navItem.index

                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 8
                        color: current ? Theme.navActive
                             : (navMouse.containsMouse ? Theme.hover : "transparent")

                        Rectangle {
                            width: 3
                            height: 18
                            radius: 2
                            anchors.left: parent.left
                            anchors.leftMargin: -12
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.accent
                            opacity: navItem.current ? 1 : 0
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: root.compact ? 14 : 12
                            anchors.rightMargin: 10
                            spacing: 11

                            Text {
                                text: navItem.modelData.glyph
                                color: navItem.current ? Theme.accent : Theme.muted
                                font.pixelSize: 14
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: !root.compact
                                text: I18n.t(navItem.modelData.key)
                                color: navItem.current ? Theme.text : Theme.mutedLight
                                font.pixelSize: 13
                                font.weight: navItem.current ? Font.DemiBold : Font.Normal
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                width: 18
                                height: 18
                                radius: 9
                                visible: !root.compact && navItem.index === 1 && backend.storeCount > 0
                                color: Theme.chip

                                Text {
                                    anchors.centerIn: parent
                                    text: backend.storeCount
                                    color: Theme.muted
                                    font.pixelSize: 9
                                }
                            }
                            Rectangle {
                                width: 18
                                height: 18
                                radius: 9
                                visible: !root.compact && navItem.index === 2 && backend.installedCount > 0
                                color: Theme.chip

                                Text {
                                    anchors.centerIn: parent
                                    text: backend.installedCount
                                    color: Theme.muted
                                    font.pixelSize: 9
                                }
                            }
                        }

                        MouseArea {
                            id: navMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.currentPage = navItem.index
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // runtime status card
                Rectangle {
                    Layout.fillWidth: true
                    visible: !root.compact
                    implicitHeight: 66
                    radius: 10
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 11
                        spacing: 3

                        RowLayout {
                            spacing: 7

                            Rectangle {
                                width: 7
                                height: 7
                                radius: 4
                                color: backend.runtimeCount > 0 ? Theme.success : Theme.warning
                            }
                            Text {
                                text: backend.runtimeCount > 0 ? I18n.t("shell.runtimeReady")
                                                               : I18n.t("shell.runtimeMissing")
                                color: Theme.text
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "v" + backend.appVersion + "  ·  Qt " + backend.qtVersion
                            color: Theme.muted
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: backend.platformName
                            color: Theme.muted
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        // ------------------------------------------------ content column
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // top bar
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 68
                color: Theme.bg

                Rectangle {
                    width: parent.width
                    height: 1
                    anchors.bottom: parent.bottom
                    color: Theme.border
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 30
                    anchors.rightMargin: 24
                    spacing: 14

                    ColumnLayout {
                        spacing: 1

                        Text {
                            text: root.pageTitle
                            color: Theme.text
                            font.pixelSize: 20
                            font.bold: true
                        }
                        Text {
                            Layout.maximumWidth: 560
                            visible: !root.compact
                            text: I18n.t(root.pageHints[backend.currentPage] || "home.subtitle")
                            color: Theme.muted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        visible: !root.compact
                        implicitHeight: 32
                        implicitWidth: summaryRow.implicitWidth + 26
                        radius: 16
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            id: summaryRow
                            anchors.centerIn: parent
                            spacing: 8

                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: backend.busy ? Theme.warning : Theme.accent
                            }

                            Text {
                                text: I18n.f("shell.installedApps",
                                             I18n.apps(backend.installedCount),
                                             I18n.repositories(backend.repositoryCount))
                                color: Theme.muted
                                font.pixelSize: 11
                            }
                        }
                    }

                    NappButton {
                        text: backend.darkMode ? "\u263C" : "\u263E"
                        small: true
                        tooltip: I18n.t("shell.themeToggle")
                        onClicked: backend.setTheme(backend.darkMode ? "light" : "dark")
                    }

                    NappButton {
                        text: "+"
                        variant: "primary"
                        small: true
                        tooltip: I18n.t("common.installPackage")
                        onClicked: backend.installFromFile()
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: backend.currentPage

                HomePage {}
                StorePage {}
                LibraryPage {}
                RuntimePage {}
                SettingsPage {}
                AboutPage {}
            }

            // status bar
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 32
                color: Theme.bgElevated

                Rectangle {
                    width: parent.width
                    height: 1
                    anchors.top: parent.top
                    color: Theme.border
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 30
                    anchors.rightMargin: 24
                    spacing: 14

                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: backend.busy ? Theme.warning : Theme.success
                    }

                    Text {
                        Layout.fillWidth: true
                        text: backend.statusText
                        color: Theme.muted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    NappProgress {
                        Layout.preferredWidth: 200
                        visible: backend.busy
                        value: backend.progress
                    }
                }
            }
        }
    }

    // ------------------------------------------------ app detail overlay
    Rectangle {
        id: detailOverlay

        readonly property var versions: backend.detail && backend.detail.versions
                                        ? backend.detail.versions : []
        readonly property var selected: versions.length > 0
                                        ? versions[Math.max(0, versionBox.currentIndex)] : null
        readonly property var installedVersion: backend.detail
                                                && backend.detail.installedVersion
                                                ? backend.detail.installedVersion : ""

        anchors.fill: parent
        visible: backend.detailVisible
        color: "#c207080b"

        MouseArea {
            anchors.fill: parent
            onClicked: backend.detailVisible = false
        }

        Rectangle {
            width: Math.min(parent.width - (root.compact ? 48 : 120), 820)
            height: Math.min(parent.height - (root.compact ? 48 : 100), 660)
            anchors.centerIn: parent
            radius: 16
            color: Theme.surface
            border.width: 1
            border.color: Theme.borderStrong

            MouseArea { anchors.fill: parent }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 26
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 18

                    AppCover {
                        width: 84
                        height: 84
                        title: backend.detail.name || ""
                        seed: (backend.detail.appId || "").length
                        source: backend.detail.iconUrl || ""
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            Layout.fillWidth: true
                            text: backend.detail.name || ""
                            color: Theme.text
                            font.pixelSize: 23
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: (backend.detail.appId || "") + "  ·  " + (backend.detail.author || "")
                                  + "  ·  " + (backend.detail.category || "")
                            color: Theme.muted
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            spacing: 6

                            Rectangle {
                                implicitHeight: 20
                                implicitWidth: repoLabel.implicitWidth + 14
                                radius: 10
                                color: Theme.chip

                                Text {
                                    id: repoLabel
                                    anchors.centerIn: parent
                                    text: backend.detail.repository || ""
                                    color: Theme.muted
                                    font.pixelSize: 10
                                }
                            }

                            Rectangle {
                                visible: detailOverlay.installedVersion.length > 0
                                implicitHeight: 20
                                implicitWidth: installedLabel.implicitWidth + 14
                                radius: 10
                                color: Theme.accentSoft

                                Text {
                                    id: installedLabel
                                    anchors.centerIn: parent
                                    text: "v" + detailOverlay.installedVersion
                                    color: Theme.accent
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }

                    NappButton {
                        text: "\u2715"
                        small: true
                        variant: "ghost"
                        onClicked: backend.detailVisible = false
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.border
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: I18n.t("detail.selectVersion")
                        color: Theme.muted
                        font.pixelSize: 12
                    }

                    NappComboBox {
                        id: versionBox
                        Layout.preferredWidth: 260
                        model: detailOverlay.versions
                        textRole: "label"
                    }

                    Text {
                        Layout.fillWidth: true
                        text: detailOverlay.selected === null ? ""
                              : (detailOverlay.selected.sizeText + "  ·  "
                                 + detailOverlay.selected.date)
                        color: Theme.muted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    NappButton {
                        text: detailOverlay.installedVersion.length > 0
                              && detailOverlay.selected !== null
                              && detailOverlay.selected.version === detailOverlay.installedVersion
                              ? I18n.t("common.manage") : I18n.t("common.install")
                        variant: "primary"
                        enabled: detailOverlay.selected !== null
                                 && detailOverlay.selected.supported
                                 && !detailOverlay.selected.yanked
                        onClicked: {
                            if (detailOverlay.selected === null)
                                return
                            if (detailOverlay.selected.version
                                    === detailOverlay.installedVersion) {
                                backend.detailVisible = false
                                backend.currentPage = 2
                                return
                            }
                            backend.installApp(backend.detail.appId,
                                               detailOverlay.selected.version)
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: detailOverlay.selected === null ? ""
                          : (detailOverlay.selected.supported
                             ? I18n.t("detail.supported")
                             : I18n.t("detail.unsupported"))
                    color: detailOverlay.selected !== null && !detailOverlay.selected.supported
                           ? Theme.danger : Theme.muted
                    font.pixelSize: 11
                }

                Text {
                    Layout.fillWidth: true
                    text: I18n.t("detail.description")
                    color: Theme.text
                    font.pixelSize: 13
                    font.bold: true
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    clip: true

                    TextArea {
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        text: backend.detail.description || ""
                        color: Theme.mutedLight
                        font.pixelSize: 12
                        background: Rectangle { color: "transparent" }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: I18n.t("detail.changelog")
                    color: Theme.text
                    font.pixelSize: 13
                    font.bold: true
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextArea {
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        color: Theme.mutedLight
                        font.pixelSize: 12
                        background: Rectangle { color: "transparent" }
                        text: detailOverlay.selected === null ? ""
                              : (detailOverlay.selected.changelog.length > 0
                                 ? detailOverlay.selected.changelog
                                 : I18n.t("detail.noChangelog"))
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 30
                    radius: 8
                    color: Theme.chip

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Text {
                            text: "\u2713"
                            color: Theme.success
                            font.pixelSize: 11
                        }
                        Text {
                            text: I18n.t("detail.trust")
                            color: Theme.muted
                            font.pixelSize: 10
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            Layout.maximumWidth: 420
                            text: detailOverlay.selected === null ? ""
                                                                  : detailOverlay.selected.sha256
                            color: Theme.muted
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }
                    }
                }
            }
        }
    }
}
