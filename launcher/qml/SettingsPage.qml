import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: page

    function themeIndex() {
        if (backend.themeMode === "light")
            return 1
        if (backend.themeMode === "dark")
            return 2
        return 0
    }

    function languageIndex() {
        return backend.language === "ko" ? 1 : 0
    }

    Connections {
        target: backend
        function onThemeChanged() {
            themeBox.currentIndex = page.themeIndex()
        }
        function onLanguageChanged() {
            languageBox.currentIndex = page.languageIndex()
        }
    }

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

            // ------------------------------------------------ appearance
            SectionHeader {
                Layout.fillWidth: true
                title: I18n.t("settings.appearance")
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 13
                color: Theme.surface
                border.width: 1
                border.color: Theme.border
                implicitHeight: appearance.implicitHeight + 34

                ColumnLayout {
                    id: appearance
                    anchors.fill: parent
                    anchors.margins: 17
                    spacing: 16

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        ColumnLayout {
                            spacing: 2
                            Layout.preferredWidth: 240
                            Text {
                                text: I18n.t("settings.theme")
                                color: Theme.text
                                font.pixelSize: 13
                            }
                            Text {
                                Layout.fillWidth: true
                                text: I18n.t("settings.themeHint")
                                color: Theme.muted
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                            }
                        }

                        Item { Layout.fillWidth: true }

                        NappComboBox {
                            id: themeBox
                            Layout.preferredWidth: 200
                            model: [
                                { label: I18n.t("settings.theme.system"), value: "system" },
                                { label: I18n.t("settings.theme.light"), value: "light" },
                                { label: I18n.t("settings.theme.dark"), value: "dark" }
                            ]
                            textRole: "label"
                            Component.onCompleted: currentIndex = page.themeIndex()
                            onActivated: backend.setTheme(model[currentIndex].value)
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.border
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        ColumnLayout {
                            spacing: 2
                            Layout.preferredWidth: 240
                            Text {
                                text: I18n.t("settings.accent")
                                color: Theme.text
                                font.pixelSize: 13
                            }
                            Text {
                                Layout.fillWidth: true
                                text: I18n.t("settings.accentHint")
                                color: Theme.muted
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                            }
                        }

                        Item { Layout.fillWidth: true }

                        RowLayout {
                            spacing: 10

                            Repeater {
                                model: ["blue", "violet", "mint", "amber"]

                                delegate: Rectangle {
                                    id: swatch

                                    required property string modelData

                                    readonly property color tone: modelData === "violet" ? "#8b5cf6"
                                                                  : modelData === "mint" ? "#1fc4a4"
                                                                  : modelData === "amber" ? "#e9a23b"
                                                                  : "#3d7df0"

                                    width: 30
                                    height: 30
                                    radius: 15
                                    color: tone
                                    border.width: backend.accent === modelData ? 2 : 0
                                    border.color: Theme.text

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 34
                                        height: 34
                                        radius: 17
                                        color: "transparent"
                                        border.width: 1
                                        border.color: swatchMouse.containsMouse
                                                      ? Theme.borderStrong : "transparent"
                                    }

                                    MouseArea {
                                        id: swatchMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: backend.setAccent(swatch.modelData)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.border
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        ColumnLayout {
                            spacing: 2
                            Layout.preferredWidth: 240
                            Text {
                                text: I18n.t("settings.language")
                                color: Theme.text
                                font.pixelSize: 13
                            }
                            Text {
                                Layout.fillWidth: true
                                text: I18n.t("settings.languageHint")
                                color: Theme.muted
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                            }
                        }

                        Item { Layout.fillWidth: true }

                        NappComboBox {
                            id: languageBox
                            Layout.preferredWidth: 200
                            model: [
                                { label: "English", value: "en" },
                                { label: "\uD55C\uAD6D\uC5B4", value: "ko" }
                            ]
                            textRole: "label"
                            Component.onCompleted: currentIndex = page.languageIndex()
                            onActivated: backend.setLanguage(model[currentIndex].value)
                        }
                    }
                }
            }

            // ------------------------------------------------ behaviour
            SectionHeader {
                Layout.fillWidth: true
                Layout.topMargin: 6
                title: I18n.t("settings.behavior")
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 13
                color: Theme.surface
                border.width: 1
                border.color: Theme.border
                implicitHeight: 72

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 17
                    spacing: 16

                    ColumnLayout {
                        spacing: 2
                        Layout.preferredWidth: 300
                        Text {
                            text: I18n.t("settings.checkUpdates")
                            color: Theme.text
                            font.pixelSize: 13
                        }
                        Text {
                            Layout.fillWidth: true
                            text: I18n.t("settings.checkUpdatesHint")
                            color: Theme.muted
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                        }
                    }

                    Item { Layout.fillWidth: true }

                    NappCheckBox {
                        checked: backend.checkUpdates
                        onToggled: backend.setCheckUpdates(checked)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 13
                color: Theme.surface
                border.width: 1
                border.color: Theme.border
                implicitHeight: 72

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 17
                    spacing: 16

                    ColumnLayout {
                        spacing: 2
                        Layout.preferredWidth: 300
                        Text {
                            text: I18n.t("settings.associateIpkg")
                            color: Theme.text
                            font.pixelSize: 13
                        }
                        Text {
                            Layout.fillWidth: true
                            text: I18n.t("settings.associateIpkgHint")
                            color: Theme.muted
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                        }
                    }

                    Item { Layout.fillWidth: true }

                    NappCheckBox {
                        checked: backend.associateIpkg
                        onToggled: backend.setAssociateIpkg(checked)
                    }
                }
            }

            // ------------------------------------------------ repositories
            SectionHeader {
                Layout.fillWidth: true
                Layout.topMargin: 6
                title: I18n.t("settings.repositories")
                note: backend.repositoryCount > 0 ? backend.repositoryCount : ""
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: backend.repositoryModel

                    delegate: Rectangle {
                        id: repoCard

                        required property string id
                        required property string name
                        required property string baseUrl
                        required property bool repoEnabled
                        required property bool official

                        Layout.fillWidth: true
                        implicitHeight: 68
                        radius: 11
                        color: Theme.surface
                        border.width: 1
                        border.color: Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 15
                            anchors.rightMargin: 12
                            spacing: 12

                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: repoCard.repoEnabled ? Theme.success : Theme.muted
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    spacing: 8

                                    Text {
                                        text: repoCard.name
                                        color: Theme.text
                                        font.pixelSize: 13
                                        font.bold: true
                                    }

                                    Rectangle {
                                        visible: repoCard.official
                                        implicitHeight: 18
                                        implicitWidth: officialLabel.implicitWidth + 14
                                        radius: 9
                                        color: Theme.accentSoft

                                        Text {
                                            id: officialLabel
                                            anchors.centerIn: parent
                                            text: I18n.t("settings.official")
                                            color: Theme.accent
                                            font.pixelSize: 9
                                        }
                                    }

                                    Rectangle {
                                        visible: !repoCard.repoEnabled
                                        implicitHeight: 18
                                        implicitWidth: offLabel.implicitWidth + 14
                                        radius: 9
                                        color: Theme.chip

                                        Text {
                                            id: offLabel
                                            anchors.centerIn: parent
                                            text: I18n.t("settings.disabled")
                                            color: Theme.muted
                                            font.pixelSize: 9
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: repoCard.baseUrl
                                    color: Theme.muted
                                    font.pixelSize: 11
                                    elide: Text.ElideMiddle
                                }
                            }

                            NappButton {
                                text: repoCard.repoEnabled ? I18n.t("common.disable")
                                                           : I18n.t("common.enable")
                                small: true
                                onClicked: backend.toggleRepository(repoCard.id)
                            }
                            NappButton {
                                text: "\u2715"
                                small: true
                                variant: "danger"
                                tooltip: I18n.t("common.delete")
                                enabled: !repoCard.official
                                onClicked: backend.removeRepository(repoCard.id)
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: 13
                color: Theme.surface
                border.width: 1
                border.color: Theme.border
                implicitHeight: addRow.implicitHeight + 34

                RowLayout {
                    id: addRow
                    anchors.fill: parent
                    anchors.margins: 17
                    spacing: 10

                    NappTextField {
                        id: repoName
                        Layout.preferredWidth: 200
                        placeholderText: I18n.t("settings.repoName")
                    }

                    NappTextField {
                        id: repoUrl
                        Layout.fillWidth: true
                        placeholderText: I18n.t("settings.repoUrl")
                    }

                    NappButton {
                        text: I18n.t("settings.addRepository")
                        variant: "primary"
                        onClicked: {
                            const message = backend.addRepository(repoName.text, repoUrl.text)
                            if (message.length > 0) {
                                console.warn("addRepository:", message)
                            } else {
                                repoName.text = ""
                                repoUrl.text = ""
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------ about pointer
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 6
                radius: 13
                color: Theme.chip
                implicitHeight: 62

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 12

                    Text {
                        text: "\u24D8"
                        color: Theme.muted
                        font.pixelSize: 14
                    }

                    Text {
                        Layout.fillWidth: true
                        text: I18n.t("settings.aboutHint")
                        color: Theme.muted
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }

                    NappButton {
                        text: I18n.t("nav.about")
                        small: true
                        onClicked: backend.currentPage = 5
                    }
                }
            }
        }
    }
}
