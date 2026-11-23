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

            // toolbar
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                NappTextField {
                    id: searchField
                    Layout.preferredWidth: 300
                    placeholderText: I18n.t("store.searchPlaceholder")
                    onTextEdited: backend.setStoreQuery(text)
                }

                NappComboBox {
                    id: categoryBox
                    Layout.preferredWidth: 176
                    model: [I18n.t("store.allCategories")].concat(backend.categories())
                    onActivated: backend.setStoreCategory(currentIndex === 0 ? "" : currentText)
                }

                Item { Layout.fillWidth: true }

                NappButton {
                    text: I18n.t("common.refresh")
                    onClicked: backend.refreshStore()
                }
            }

            // hero
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 168
                radius: 16
                clip: true

                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.heroTop }
                    GradientStop { position: 1.0; color: Theme.heroBottom }
                }

                Rectangle {
                    width: 420
                    height: 420
                    radius: 210
                    x: parent.width - 280
                    y: -230
                    color: Theme.accent
                    opacity: Theme.dark ? 0.22 : 0.14
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 32
                    anchors.rightMargin: 32
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Text {
                        text: I18n.t("store.eyebrow")
                        color: Theme.heroSub
                        font.pixelSize: 10
                        font.letterSpacing: 2.6
                    }

                    Text {
                        Layout.fillWidth: true
                        text: backend.storeCount > 0
                              ? I18n.f("store.title", backend.storeCount)
                              : I18n.t("store.emptyTitle")
                        color: Theme.heroText
                        font.pixelSize: 25
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 640
                        text: backend.storeStatusText
                        color: Theme.heroSub
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        spacing: 10
                        Layout.topMargin: 6

                        NappButton {
                            text: I18n.t("store.manageRepos")
                            variant: "primary"
                            onClicked: backend.currentPage = 4
                        }
                        NappButton {
                            text: I18n.t("common.installPackage")
                            onClicked: backend.installFromFile()
                        }
                    }
                }
            }

            SectionHeader {
                Layout.fillWidth: true
                Layout.topMargin: 4
                title: I18n.t("store.allApps")
                note: backend.storeCount > 0 ? backend.storeCount : ""
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: backend.storeCount > 0

                Repeater {
                    model: backend.storeModel

                    delegate: Rectangle {
                        id: card

                        required property string appId
                        required property string name
                        required property string description
                        required property string latest
                        required property string installedVersion
                        required property bool updateAvailable
                        required property bool supported

                        Layout.fillWidth: true
                        implicitHeight: 96
                        radius: 12
                        color: cardMouse.containsMouse ? Theme.hover : Theme.surface
                        border.width: 1
                        border.color: cardMouse.containsMouse ? Theme.accentLine : Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 15
                            spacing: 15

                            AppCover {
                                width: 66
                                height: 66
                                title: card.name
                                seed: card.appId.length
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                RowLayout {
                                    spacing: 8

                                    Text {
                                        text: card.name
                                        color: Theme.text
                                        font.pixelSize: 14
                                        font.bold: true
                                    }

                                    Rectangle {
                                        implicitHeight: 19
                                        implicitWidth: versionLabel.implicitWidth + 16
                                        radius: 9
                                        color: Theme.chip

                                        Text {
                                            id: versionLabel
                                            anchors.centerIn: parent
                                            text: "v" + card.latest
                                            color: Theme.muted
                                            font.pixelSize: 10
                                        }
                                    }

                                    Rectangle {
                                        visible: card.updateAvailable
                                        implicitHeight: 19
                                        implicitWidth: updateLabel.implicitWidth + 16
                                        radius: 9
                                        color: Theme.accentSoft

                                        Text {
                                            id: updateLabel
                                            anchors.centerIn: parent
                                            text: I18n.t("common.update")
                                            color: Theme.accent
                                            font.pixelSize: 10
                                        }
                                    }

                                    Item { Layout.fillWidth: true }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: card.description.length > 0 ? card.description
                                                                      : card.appId
                                    color: Theme.muted
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: !card.supported ? I18n.t("store.unsupported")
                                          : card.installedVersion.length === 0
                                            ? I18n.t("common.notInstalled")
                                          : card.updateAvailable
                                            ? I18n.f("store.updateAvailable",
                                                     card.installedVersion)
                                          : I18n.f("store.installed", card.installedVersion)
                                    color: !card.supported ? Theme.danger
                                          : card.updateAvailable ? Theme.accent : Theme.muted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            NappButton {
                                text: card.installedVersion.length === 0
                                      ? I18n.t("common.install")
                                      : (card.updateAvailable ? I18n.t("common.update")
                                                              : I18n.t("common.manage"))
                                variant: card.installedVersion.length === 0
                                         || card.updateAvailable ? "primary" : "secondary"
                                onClicked: {
                                    if (card.updateAvailable)
                                        backend.showAppDetails(card.appId)
                                    else if (card.installedVersion.length > 0)
                                        backend.currentPage = 2
                                    else
                                        backend.showAppDetails(card.appId)
                                }
                            }
                        }

                        MouseArea {
                            id: cardMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            // Behind the row so the action button keeps its clicks.
                            z: -1
                            onClicked: backend.showAppDetails(card.appId)
                        }
                    }
                }
            }

            EmptyState {
                Layout.fillWidth: true
                Layout.topMargin: 28
                Layout.bottomMargin: 28
                visible: backend.storeCount === 0
                glyph: "\u25A6"
                title: I18n.t("store.emptyListTitle")
                description: I18n.t("store.emptyListDescription")
            }
        }
    }
}
