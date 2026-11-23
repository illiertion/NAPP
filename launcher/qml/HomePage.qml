import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: page

    readonly property int columnCount: Math.max(1, Math.floor((scroll.availableWidth - 60) / 290))
    readonly property real cellWidth: Math.max(250, (scroll.availableWidth - 60
                                                       - (page.columnCount - 1) * 12)
                                                      / page.columnCount)

    ScrollView {
        id: scroll

        anchors.fill: parent
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        clip: true

        ColumnLayout {
            x: 30
            y: 26
            width: Math.max(1, scroll.availableWidth - 60)
            spacing: 18

            // ---------------------------------------------------- hero
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 208
                radius: 16
                clip: true

                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.heroTop }
                    GradientStop { position: 1.0; color: Theme.heroBottom }
                }

                Rectangle {
                    width: 520
                    height: 520
                    radius: 260
                    x: parent.width - 340
                    y: -280
                    color: Theme.accent
                    opacity: Theme.dark ? 0.22 : 0.14
                }

                Rectangle {
                    width: 260
                    height: 260
                    radius: 130
                    x: parent.width - 620
                    y: 110
                    color: Theme.accent
                    opacity: Theme.dark ? 0.08 : 0.06
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 34
                    anchors.rightMargin: 34
                    spacing: 26

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 9

                        Text {
                            text: I18n.t("home.eyebrow")
                            color: Theme.heroSub
                            font.pixelSize: 10
                            font.letterSpacing: 2.6
                        }

                        Text {
                            Layout.maximumWidth: 620
                            text: I18n.t("home.title")
                            color: Theme.heroText
                            font.pixelSize: 31
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.maximumWidth: 560
                            text: I18n.t("home.subtitle")
                            color: Theme.heroSub
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            spacing: 10
                            Layout.topMargin: 8

                            NappButton {
                                text: I18n.t("common.browseStore")
                                variant: "primary"
                                onClicked: backend.currentPage = 1
                            }
                            NappButton {
                                text: I18n.t("common.installPackage")
                                onClicked: backend.installFromFile()
                            }
                        }
                    }

                    // brand plate
                    Rectangle {
                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 148
                        radius: 14
                        color: Qt.rgba(1, 1, 1, Theme.dark ? 0.04 : 0.55)
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, Theme.dark ? 0.10 : 0.7)

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 6

                            Image {
                                Layout.preferredWidth: 54
                                Layout.preferredHeight: 54
                                source: "qrc:/branding/napp-icon-256.png"
                                sourceSize.width: 108
                                sourceSize.height: 108
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                            }

                            Item { Layout.fillHeight: true }

                            Text {
                                text: "NAPP " + backend.appVersion
                                color: Theme.heroText
                                font.pixelSize: 13
                                font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: backend.platformName
                                color: Theme.heroSub
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------- stats
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Repeater {
                    model: [
                        { label: I18n.t("home.statApps"), hint: I18n.t("home.statAppsHint"), value: backend.installedCount, accent: false },
                        { label: I18n.t("home.statUpdates"), hint: I18n.t("home.statUpdatesHint"), value: backend.updatesCount, accent: backend.updatesCount > 0 },
                        { label: I18n.t("home.statStore"), hint: I18n.t("home.statStoreHint"), value: backend.storeCount, accent: false },
                        { label: I18n.t("home.statRuntime"), hint: I18n.t("home.statRuntimeHint"), value: backend.runtimeCount, accent: false }
                    ]

                    delegate: Rectangle {
                        id: stat

                        required property var modelData

                        Layout.fillWidth: true
                        implicitHeight: 74
                        radius: 12
                        color: Theme.surface
                        border.width: 1
                        border.color: stat.modelData.accent ? Theme.accentLine : Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 12

                            Text {
                                text: stat.modelData.value
                                color: stat.modelData.accent ? Theme.accent : Theme.text
                                font.pixelSize: 24
                                font.bold: true
                            }

                            ColumnLayout {
                                spacing: 0
                                Text {
                                    text: stat.modelData.label
                                    color: Theme.muted
                                    font.pixelSize: 11
                                }
                                Text {
                                    text: stat.modelData.hint
                                    color: Theme.muted
                                    opacity: 0.7
                                    font.pixelSize: 9
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }

            // ---------------------------------------------------- updates
            SectionHeader {
                Layout.fillWidth: true
                Layout.topMargin: 4
                title: I18n.t("home.updates")
                note: backend.updatesCount > 0 ? backend.updatesCount : ""
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: backend.updatesCount > 0

                Repeater {
                    model: backend.updatesModel

                    delegate: Rectangle {
                        id: updateRow

                        required property string appId
                        required property string name
                        required property string fromVersion
                        required property string toVersion

                        Layout.fillWidth: true
                        implicitHeight: 60
                        radius: 11
                        color: Theme.surface
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 12
                            spacing: 12

                            AppCover {
                                width: 36
                                height: 36
                                title: updateRow.name
                                seed: updateRow.appId.length
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    text: updateRow.name
                                    color: Theme.text
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                Text {
                                    text: "v" + updateRow.fromVersion + "  \u2192  v"
                                          + updateRow.toVersion
                                    color: Theme.muted
                                    font.pixelSize: 11
                                }
                            }

                            NappButton {
                                text: I18n.t("common.update")
                                small: true
                                variant: "primary"
                                onClicked: backend.installApp(updateRow.appId,
                                                              updateRow.toVersion)
                            }
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: I18n.t("home.noUpdates")
                color: Theme.muted
                font.pixelSize: 12
                visible: backend.updatesCount === 0
            }

            // ---------------------------------------------------- installed
            SectionHeader {
                Layout.fillWidth: true
                Layout.topMargin: 6
                title: I18n.t("home.installed")
                note: backend.installedCount
            }

            GridLayout {
                Layout.fillWidth: true
                columns: page.columnCount
                columnSpacing: 12
                rowSpacing: 12
                visible: backend.installedCount > 0

                Repeater {
                    model: backend.installedModel

                    delegate: Rectangle {
                        id: appTile

                        required property string appId
                        required property string name
                        required property string version
                        required property bool running
                        required property string iconUrl

                        Layout.preferredWidth: page.cellWidth
                        Layout.preferredHeight: 100
                        radius: 12
                        color: hover.containsMouse ? Theme.hover : Theme.surface
                        border.width: 1
                        border.color: appTile.running ? Theme.accentLine : Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 13
                            spacing: 12

                            AppCover {
                                width: 58
                                height: 58
                                title: appTile.name
                                seed: appTile.appId.length
                                source: appTile.iconUrl
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: appTile.name
                                    color: Theme.text
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "v" + appTile.version
                                          + (appTile.running ? "  ·  " + I18n.t("common.running")
                                                             : "")
                                    color: appTile.running ? Theme.accent : Theme.muted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    spacing: 6
                                    Layout.topMargin: 4

                                    NappButton {
                                        text: appTile.running ? I18n.t("common.stop")
                                                              : I18n.t("common.launch")
                                        small: true
                                        variant: "primary"
                                        onClicked: appTile.running
                                                   ? backend.stopApp(appTile.appId)
                                                   : backend.launchApp(appTile.appId)
                                    }
                                    NappButton {
                                        text: I18n.t("common.manage")
                                        small: true
                                        variant: "ghost"
                                        onClicked: backend.currentPage = 2
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: hover
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }
            }

            EmptyState {
                Layout.fillWidth: true
                Layout.topMargin: 22
                Layout.bottomMargin: 22
                visible: backend.installedCount === 0
                glyph: "\u25A4"
                title: I18n.t("home.emptyTitle")
                description: I18n.t("home.emptyDescription")
            }
        }
    }
}
