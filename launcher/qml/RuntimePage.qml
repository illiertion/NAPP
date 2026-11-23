import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: page

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
            spacing: 16

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 132
                radius: 14
                color: Theme.surface
                border.width: 1
                border.color: backend.runtimeCount > 0 ? Theme.accentLine : Theme.border

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 18

                    Rectangle {
                        width: 48
                        height: 48
                        radius: 12
                        color: backend.runtimeCount > 0 ? Theme.accentSoft : Theme.surfaceAlt

                        Text {
                            anchors.centerIn: parent
                            text: "\u25C8"
                            color: backend.runtimeCount > 0 ? Theme.accent : Theme.muted
                            font.pixelSize: 20
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Text {
                            text: backend.runtimeCount > 0 ? I18n.t("runtime.ready")
                                                           : I18n.t("runtime.missing")
                            color: Theme.text
                            font.pixelSize: 17
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: backend.runtimeStatusText
                            color: Theme.muted
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            spacing: 10
                            Layout.topMargin: 4

                            NappButton {
                                text: I18n.t("runtime.register")
                                variant: "primary"
                                small: true
                                onClicked: backend.registerRuntimeFolder()
                            }
                            NappButton {
                                text: I18n.t("runtime.openFolder")
                                small: true
                                onClicked: backend.openRuntimeFolder()
                            }
                        }
                    }
                }
            }

            SectionHeader {
                Layout.fillWidth: true
                Layout.topMargin: 4
                title: I18n.t("runtime.installed")
                note: backend.runtimeCount > 0 ? backend.runtimeCount : ""
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: backend.runtimeCount > 0

                Repeater {
                    model: backend.runtimeModel

                    delegate: Rectangle {
                        id: runtimeCard

                        required property string version
                        required property string chromiumVersion
                        required property bool active
                        required property bool valid
                        required property string problem
                        required property string directory
                        required property string stateLabel

                        Layout.fillWidth: true
                        implicitHeight: 84
                        radius: 12
                        color: Theme.surface
                        border.width: 1
                        border.color: runtimeCard.active ? Theme.accentLine : Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 14

                            Rectangle {
                                width: 4
                                height: 44
                                radius: 2
                                color: !runtimeCard.valid ? Theme.danger
                                     : runtimeCard.active ? Theme.accent : Theme.border
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: "v" + runtimeCard.version + "  ·  "
                                          + I18n.t("runtime.chromium") + " "
                                          + runtimeCard.chromiumVersion
                                    color: Theme.text
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: runtimeCard.valid
                                          ? runtimeCard.directory
                                          : (runtimeCard.problem.length > 0 ? runtimeCard.problem
                                                                            : runtimeCard.directory)
                                    color: runtimeCard.valid ? Theme.muted : Theme.danger
                                    font.pixelSize: 11
                                    elide: Text.ElideMiddle
                                }
                            }

                            Rectangle {
                                implicitHeight: 20
                                implicitWidth: stateText.implicitWidth + 16
                                radius: 10
                                color: !runtimeCard.valid ? Theme.surfaceAlt
                                     : runtimeCard.active ? Theme.accentSoft : Theme.chip

                                Text {
                                    id: stateText
                                    anchors.centerIn: parent
                                    text: runtimeCard.stateLabel
                                    color: !runtimeCard.valid ? Theme.danger
                                         : runtimeCard.active ? Theme.accent : Theme.muted
                                    font.pixelSize: 10
                                }
                            }

                            NappButton {
                                text: I18n.t("runtime.activate")
                                small: true
                                enabled: runtimeCard.valid && !runtimeCard.active
                                onClicked: backend.activateRuntime(runtimeCard.version)
                            }
                            NappButton {
                                text: "\u2715"
                                small: true
                                variant: "danger"
                                tooltip: I18n.t("common.remove")
                                enabled: !runtimeCard.active
                                onClicked: backend.removeRuntime(runtimeCard.version)
                            }
                        }
                    }
                }
            }

            EmptyState {
                Layout.fillWidth: true
                Layout.topMargin: 22
                Layout.bottomMargin: 22
                visible: backend.runtimeCount === 0
                glyph: "\u25C8"
                title: I18n.t("runtime.emptyTitle")
                description: I18n.t("runtime.emptyDescription")
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 6
                implicitHeight: noteLayout.implicitHeight + 26
                radius: 11
                color: Theme.chip

                RowLayout {
                    id: noteLayout
                    anchors.fill: parent
                    anchors.margins: 13
                    spacing: 10

                    Text {
                        text: "\u26A0"
                        color: Theme.warning
                        font.pixelSize: 13
                    }

                    Text {
                        Layout.fillWidth: true
                        text: I18n.t("runtime.note")
                        color: Theme.muted
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
