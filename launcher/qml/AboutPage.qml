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

            // ------------------------------------------------ identity
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 168
                radius: 16
                clip: true

                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.heroTop }
                    GradientStop { position: 1.0; color: Theme.heroBottom }
                }

                Rectangle {
                    width: 380
                    height: 380
                    radius: 190
                    x: parent.width - 250
                    y: -200
                    color: Theme.accent
                    opacity: Theme.dark ? 0.2 : 0.12
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 32
                    anchors.rightMargin: 32
                    spacing: 22

                    Rectangle {
                        width: 92
                        height: 92
                        radius: 18
                        color: "#07080b"
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.12)
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: "qrc:/branding/napp-icon-256.png"
                            sourceSize.width: 184
                            sourceSize.height: 184
                            fillMode: Image.PreserveAspectCrop
                            smooth: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Text {
                            text: I18n.t("about.title")
                            color: Theme.heroText
                            font.pixelSize: 26
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.maximumWidth: 560
                            text: I18n.t("about.subtitle")
                            color: Theme.heroSub
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }

                        RowLayout {
                            spacing: 8

                            Rectangle {
                                implicitHeight: 24
                                implicitWidth: versionChip.implicitWidth + 18
                                radius: 12
                                color: Qt.rgba(1, 1, 1, Theme.dark ? 0.10 : 0.65)

                                Text {
                                    id: versionChip
                                    anchors.centerIn: parent
                                    text: I18n.t("about.version") + " " + backend.appVersion
                                    color: Theme.heroText
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }

                            Rectangle {
                                implicitHeight: 24
                                implicitWidth: buildChip.implicitWidth + 18
                                radius: 12
                                color: Qt.rgba(1, 1, 1, Theme.dark ? 0.08 : 0.55)

                                Text {
                                    id: buildChip
                                    anchors.centerIn: parent
                                    text: backend.buildCompiler + "  ·  " + backend.platformName
                                    color: Theme.heroSub
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------ build facts
            SectionHeader {
                Layout.fillWidth: true
                title: I18n.t("about.environment")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 12
                rowSpacing: 12

                Repeater {
                    model: [
                        { label: I18n.t("about.version"), value: backend.appVersion },
                        { label: I18n.t("about.qt"), value: backend.qtVersion },
                        { label: I18n.t("about.cef"), value: backend.cefVersion },
                        { label: I18n.t("about.compiler"), value: backend.buildCompiler },
                        { label: I18n.t("about.platform"), value: backend.platformName },
                        { label: I18n.t("about.built"), value: backend.buildDate }
                    ]

                    delegate: Rectangle {
                        id: fact

                        required property var modelData

                        Layout.fillWidth: true
                        implicitHeight: 66
                        radius: 11
                        color: Theme.surface
                        border.width: 1
                        border.color: Theme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 13
                            spacing: 2

                            Text {
                                text: fact.modelData.label
                                color: Theme.muted
                                font.pixelSize: 10
                                font.letterSpacing: 0.6
                            }
                            Text {
                                Layout.fillWidth: true
                                text: fact.modelData.value
                                color: Theme.text
                                font.pixelSize: 13
                                font.bold: true
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------ licences
            SectionHeader {
                Layout.fillWidth: true
                Layout.topMargin: 6
                title: I18n.t("about.licenses")
            }

            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: 720
                text: I18n.t("about.licensesHint")
                color: Theme.muted
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: backend.licensesModel

                    delegate: Rectangle {
                        id: licenseCard

                        required property string name
                        required property string version
                        required property string license
                        required property string url
                        required property string note

                        Layout.fillWidth: true
                        implicitHeight: 76
                        radius: 11
                        color: Theme.surface
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                RowLayout {
                                    spacing: 8

                                    Text {
                                        text: licenseCard.name
                                        color: Theme.text
                                        font.pixelSize: 13
                                        font.bold: true
                                    }

                                    Rectangle {
                                        implicitHeight: 18
                                        implicitWidth: licenseLabel.implicitWidth + 14
                                        radius: 9
                                        color: Theme.chip

                                        Text {
                                            id: licenseLabel
                                            anchors.centerIn: parent
                                            text: licenseCard.license
                                            color: Theme.muted
                                            font.pixelSize: 9
                                        }
                                    }

                                    Rectangle {
                                        visible: licenseCard.version.length > 0
                                        implicitHeight: 18
                                        implicitWidth: versionLabel.implicitWidth + 14
                                        radius: 9
                                        color: Theme.chip

                                        Text {
                                            id: versionLabel
                                            anchors.centerIn: parent
                                            text: licenseCard.version
                                            color: Theme.muted
                                            font.pixelSize: 9
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: licenseCard.note
                                    color: Theme.muted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            NappButton {
                                text: I18n.t("about.openLicense")
                                small: true
                                visible: licenseCard.url.length > 0
                                onClicked: backend.openExternal(licenseCard.url)
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 10
                Layout.bottomMargin: 12
                spacing: 8

                // The ILLIERTION mark appears only here, as publisher credit.
                Image {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    source: "qrc:/branding/logo-256.png"
                    sourceSize.width: 36
                    sourceSize.height: 36
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    opacity: 0.85
                }

                Text {
                    text: "ILLIERTION"
                    color: Theme.mutedLight
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.2
                }

                Text {
                    text: "\u00A9 2026"
                    color: Theme.muted
                    font.pixelSize: 10
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "NAPP " + backend.appVersion + "  ·  Qt " + backend.qtVersion
                          + "  ·  CEF " + backend.cefVersion
                    color: Theme.muted
                    font.pixelSize: 10
                }
            }
        }
    }
}
