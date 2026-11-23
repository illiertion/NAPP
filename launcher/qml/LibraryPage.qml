import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: page

    // Cells are sized from the real space a column gets, and never wider than
    // it: a minimum width larger than the viewport was what pushed the last
    // column (and its buttons) past the right edge.
    // 칸 너비는 실제 열 공간에서 계산하고 그보다 커지지 않게 한다. 최소 너비가 화면보다
    // 커지면 마지막 열과 그 버튼이 오른쪽 밖으로 밀려나 잘렸다.
    readonly property real usableWidth: Math.max(1, scroll.availableWidth - 60)
    readonly property int columnCount: Math.max(1, Math.floor((usableWidth + 12) / 262))
    readonly property real cellWidth: Math.max(180, Math.min(page.usableWidth,
                                                             (page.usableWidth
                                                              - (page.columnCount - 1) * 12)
                                                             / page.columnCount))

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

            // header
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    spacing: 2

                    Text {
                        text: I18n.f("library.title", backend.installedCount)
                        color: Theme.text
                        font.pixelSize: 15
                        font.bold: true
                    }
                    Text {
                        text: I18n.t("library.subtitle")
                        color: Theme.muted
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                NappButton {
                    text: I18n.t("common.installPackage")
                    variant: "primary"
                    onClicked: backend.installFromFile()
                }
            }

            // toolbar
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                NappTextField {
                    Layout.preferredWidth: 260
                    placeholderText: I18n.t("library.searchPlaceholder")
                    onTextEdited: backend.setLibraryQuery(text)
                }

                Repeater {
                    model: [
                        { value: "all", label: I18n.t("library.filter.all") },
                        { value: "running", label: I18n.t("library.filter.running") },
                        { value: "updates", label: I18n.t("library.filter.updates") }
                    ]

                    delegate: NappButton {
                        required property var modelData

                        text: modelData.label
                        small: true
                        variant: backend.libraryFilter === modelData.value ? "primary"
                                                                          : "secondary"
                        onClicked: backend.setLibraryFilter(modelData.value)
                    }
                }

                Item { Layout.fillWidth: true }

                NappComboBox {
                    id: sortBox
                    Layout.preferredWidth: 176
                    model: [
                        { label: I18n.t("library.sort.name"), value: 0 },
                        { label: I18n.t("library.sort.recent"), value: 1 },
                        { label: I18n.t("library.sort.running"), value: 2 }
                    ]
                    textRole: "label"
                    currentIndex: backend.librarySort
                    onActivated: backend.setLibrarySort(model[currentIndex].value)
                }
            }

            // grid
            GridLayout {
                Layout.fillWidth: true
                columns: page.columnCount
                columnSpacing: 12
                rowSpacing: 12
                visible: backend.libraryCount > 0

                Repeater {
                    model: backend.installedModel

                    delegate: Rectangle {
                        id: tile

                        required property string appId
                        required property string name
                        required property string version
                        required property string permissionsLabel
                        required property string installedDate
                        required property bool running
                        required property bool updateAvailable
                        required property string iconUrl
                        required property bool hasCustomIcon

                        Layout.preferredWidth: page.cellWidth
                        Layout.maximumWidth: page.cellWidth
                        Layout.minimumWidth: 0
                        Layout.preferredHeight: 272
                        radius: 13
                        color: Theme.surface
                        border.width: 1
                        border.color: tileMouse.containsMouse || tile.running
                                      ? Theme.accentLine : Theme.border

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 13
                            spacing: 9

                            AppCover {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 132
                                title: tile.name
                                seed: tile.appId.length
                                source: tile.iconUrl
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    spacing: 6

                                    Text {
                                        Layout.fillWidth: true
                                        text: tile.name
                                        color: Theme.text
                                        font.pixelSize: 13
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Rectangle {
                                        visible: tile.running
                                        width: 7
                                        height: 7
                                        radius: 4
                                        color: Theme.success
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "v" + tile.version
                                          + (tile.running ? "  ·  " + I18n.t("common.running")
                                                          : "")
                                          + (tile.updateAvailable
                                             ? "  ·  " + I18n.t("common.update") : "")
                                    color: tile.updateAvailable ? Theme.accent
                                          : tile.running ? Theme.success : Theme.muted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: tile.permissionsLabel
                                    color: Theme.muted
                                    font.pixelSize: 10
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: tile.installedDate.length > 0
                                          ? I18n.f("library.installedOn", tile.installedDate) : ""
                                    color: Theme.muted
                                    opacity: 0.75
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }

                            Item { Layout.fillHeight: true }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                // minimumWidth 0 lets the row shrink with the tile
                                // (the label elides) instead of overflowing.
                                // minimumWidth 0 이면 타일이 좁아져도 버튼이 넘치지 않고
                                // 글자가 줄임표로 표시된다.
                                NappButton {
                                    text: tile.running ? I18n.t("common.stop")
                                                       : I18n.t("common.launch")
                                    small: true
                                    variant: "primary"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    onClicked: tile.running ? backend.stopApp(tile.appId)
                                                            : backend.launchApp(tile.appId)
                                }
                                NappButton {
                                    text: I18n.t("common.data")
                                    small: true
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    onClicked: backend.openDataFolder(tile.appId)
                                }
                                NappButton {
                                    text: I18n.t("common.permissions")
                                    small: true
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    onClicked: backend.editPermissions(tile.appId)
                                }
                            }

                        }

                        // The overflow menu sits on the cover instead of the button
                        // row: four buttons never fit a narrow tile.
                        // 좁은 타일에서 버튼 네 개가 넘치지 않도록 메뉴는 커버 위에 둔다.
                        NappButton {
                            id: moreButton
                            text: "\u22EF"
                            small: true
                            tooltip: I18n.t("common.more")
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: 21
                            anchors.rightMargin: 21
                            width: 30
                            height: 30
                            onClicked: tileMenu.open()
                        }

                        Menu {
                            id: tileMenu
                            y: moreButton.height + 2
                            width: 220

                            MenuItem {
                                text: I18n.t("common.changeIcon")
                                onTriggered: backend.chooseAppIcon(tile.appId)
                            }
                            MenuItem {
                                text: I18n.t("common.resetIcon")
                                enabled: tile.hasCustomIcon
                                onTriggered: backend.resetAppIcon(tile.appId)
                            }
                            MenuItem {
                                text: I18n.t("common.createShortcut")
                                onTriggered: backend.createShortcut(tile.appId)
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: I18n.t("common.remove")
                                onTriggered: backend.removeApp(tile.appId)
                            }
                        }

                        MouseArea {
                            id: tileMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }
            }

            EmptyState {
                Layout.fillWidth: true
                Layout.topMargin: 30
                Layout.bottomMargin: 30
                visible: backend.installedCount === 0
                glyph: "\u25A4"
                title: I18n.t("library.emptyTitle")
                description: I18n.t("library.emptyDescription")
            }

            EmptyState {
                Layout.fillWidth: true
                Layout.topMargin: 30
                Layout.bottomMargin: 30
                visible: backend.installedCount > 0 && backend.libraryCount === 0
                glyph: "\u2315"
                title: I18n.t("library.filterEmptyTitle")
                description: I18n.t("library.filterEmptyDescription")
            }
        }
    }
}
