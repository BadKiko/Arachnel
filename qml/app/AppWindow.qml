import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.ApplicationWindow {
    id: root

    visible: true
    width: 1440
    height: 900
    minimumWidth: 1100
    minimumHeight: 720
    title: qsTr("Arachnel")
    color: MD.Token.color.surface

    MD.MProp.textColor: MD.MProp.color.on_surface
    MD.MProp.backgroundColor: MD.MProp.color.surface

    property int windowClass: MD.Token.window_class.select_type(width)
    MD.MProp.size.windowClass: windowClass

    // Debounce window class updates during resize
    Timer {
        id: windowClassTimer
        interval: 80
        onTriggered: root.windowClass = MD.Token.window_class.select_type(root.width)
    }
    onWidthChanged: windowClassTimer.restart()

    property int pageIndex: 0
    property bool detailsOpen: false
    property string detailsGameId: ""
    property bool detailsFromCatalog: false

    function openGameDetails(gameId, fromCatalog) {
        detailsGameId = gameId
        detailsFromCatalog = !!fromCatalog
        detailsOpen = true
    }

    function closeGameDetails() {
        detailsOpen = false
        detailsGameId = ""
    }

    Component.onCompleted: Appearance.apply()

    Connections {
        target: Core
        function onLastActionChanged() {
            if (Core.lastAction.length > 0)
                snackbar.show(Core.lastAction)
        }
    }

    readonly property var navModel: [
        {
            name: qsTr("Библиотека"),
            icon: MD.Token.icon.sports_esports
        },
        {
            name: qsTr("Каталог"),
            icon: MD.Token.icon.storefront
        }
    ]

    RowLayout {
        anchors.fill: parent
        spacing: 0

        AppRail {
            id: navRail
            Layout.fillHeight: true
            model: root.navModel
            currentIndex: root.pageIndex
            onActivated: function (index) {
                root.closeGameDetails()
                root.pageIndex = index
            }
            onSettingsRequested: settingsSheet.open()
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            MD.Pane {
                Layout.fillWidth: true
                padding: MD.Token.spacing.medium
                backgroundColor: MD.Token.color.surface
                visible: !root.detailsOpen

                RowLayout {
                    width: parent.width
                    spacing: MD.Token.spacing.medium

                    MD.SearchBar {
                        id: globalSearch
                        Layout.fillWidth: true
                        Layout.maximumWidth: 720
                        Layout.alignment: Qt.AlignHCenter
                        onAccepted: {
                            root.closeGameDetails()
                            root.pageIndex = 1
                            Core.searchCatalog("online-fix", searchText)
                        }
                    }

                    Item { Layout.fillWidth: true }

                    MD.IconButton {
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.notifications
                        onClicked: snackbar.show(qsTr("Уведомлений пока нет"))
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.detailsOpen ? 2 : root.pageIndex

                LibraryPage {
                    onOpenGame: function (id) { root.openGameDetails(id, false) }
                }
                CatalogPage {
                    onOpenGame: function (id) { root.openGameDetails(id, true) }
                }
                GameDetailsPage {
                    gameId: root.detailsGameId
                    fromCatalog: root.detailsFromCatalog
                    onBackRequested: root.closeGameDetails()
                }
            }
        }
    }

    SettingsSheet {
        id: settingsSheet
        anchors.fill: parent
    }

    MD.SnakeView {
        id: snackbar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 120
        anchors.margins: MD.Token.spacing.large
        height: Math.min(implicitHeight, 160)
        bottomToTop: true
    }
}
