import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

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
    color: MD.Token.color.surface_container

    MD.MProp.textColor: MD.MProp.color.on_surface
    MD.MProp.backgroundColor: MD.MProp.color.surface_container

    property int windowClass: MD.Token.window_class.select_type(width)
    MD.MProp.size.windowClass: windowClass

    Timer {
        id: windowClassTimer
        interval: 200
        onTriggered: {
            const next = MD.Token.window_class.select_type(root.width)
            if (next !== root.windowClass)
                root.windowClass = next
        }
    }
    onWidthChanged: windowClassTimer.restart()

    property int pageIndex: 0
    property bool detailsOpen: pageStack.depth > 1
    property string detailsGameId: ""
    property bool detailsFromCatalog: false
    property string catalogSourceId: Core.sources.firstEnabledId

    function goToPage(index) {
        if (pageStack.depth > 1)
            pageStack.navigateToRoot()

        root.detailsGameId = ""
        root.pageIndex = index
        if (pageStack.currentItem && pageStack.currentItem.pageIndex !== undefined)
            pageStack.currentItem.pageIndex = index
    }

    function openGameDetails(gameId, fromCatalog) {
        root.detailsGameId = gameId
        root.detailsFromCatalog = !!fromCatalog
        pageStack.navigatePush(detailsPageComponent, {
                                   "gameId": gameId,
                                   "fromCatalog": !!fromCatalog
                               })
    }

    function closeGameDetails() {
        if (pageStack.canPop)
            pageStack.navigatePop()
        root.detailsGameId = ""
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

    Component {
        id: mainPagesComponent
        Item {
            id: mainPages

            property int pageIndex: root.pageIndex

            transformOrigin: Item.Center

            Rectangle {
                anchors.fill: parent
                color: MD.Token.color.surface
            }

            // Keep both pages alive (catalog load must not restart on tab switch).
            // Soft crossfade + light bounce on the active tab.
            LibraryPage {
                anchors.fill: parent
                opacity: mainPages.pageIndex === 0 ? 1 : 0
                scale: mainPages.pageIndex === 0 ? 1 : 0.97
                enabled: mainPages.pageIndex === 0 && opacity > 0.99
                transformOrigin: Item.Center
                onOpenGame: function (id) { root.openGameDetails(id, false) }
                onOpenCatalog: root.goToPage(1)
                onOpenSettings: settingsSheet.openSettings()
                onAddSourceRequested: settingsSheet.openSources(true)

                Behavior on opacity {
                    NumberAnimation {
                        duration: pageStack.enterDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
                Behavior on scale {
                    NumberAnimation {
                        duration: pageStack.enterDuration
                        easing.type: Easing.OutBack
                        easing.overshoot: 1.2
                    }
                }
            }

            CatalogPage {
                anchors.fill: parent
                opacity: mainPages.pageIndex === 1 ? 1 : 0
                scale: mainPages.pageIndex === 1 ? 1 : 0.97
                enabled: mainPages.pageIndex === 1 && opacity > 0.99
                transformOrigin: Item.Center
                onOpenGame: function (id) { root.openGameDetails(id, true) }
                onSelectedSourceIdChanged: root.catalogSourceId = selectedSourceId
                onOpenSettings: settingsSheet.openSettings()
                onAddSourceRequested: settingsSheet.openSources(true)

                Behavior on opacity {
                    NumberAnimation {
                        duration: pageStack.enterDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
                Behavior on scale {
                    NumberAnimation {
                        duration: pageStack.enterDuration
                        easing.type: Easing.OutBack
                        easing.overshoot: 1.2
                    }
                }
            }
        }
    }

    Component {
        id: detailsPageComponent
        GameDetailsPage {
            transformOrigin: Item.Center
            onBackRequested: root.closeGameDetails()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        AppRail {
            id: navRail
            Layout.fillHeight: true
            model: root.navModel
            currentIndex: root.pageIndex
            onActivated: function (index) { root.goToPage(index) }
            onSettingsRequested: settingsSheet.openSettings()
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: MD.Token.spacing.small
            Layout.rightMargin: MD.Token.spacing.small
            Layout.bottomMargin: MD.Token.spacing.small
            spacing: 0

            MD.Pane {
                id: mainPane
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 0
                radius: MD.Token.shape.corner.extra_large
                backgroundColor: MD.Token.color.surface
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    MD.Pane {
                        Layout.fillWidth: true
                        padding: MD.Token.spacing.medium
                        backgroundColor: "transparent"
                        visible: !root.detailsOpen
                        opacity: root.detailsOpen ? 0 : 1

                        Behavior on opacity {
                            NumberAnimation {
                                duration: pageStack.exitDuration
                                easing: MD.Token.easing.emphasized_accelerate
                            }
                        }

                        RowLayout {
                            width: parent.width
                            spacing: MD.Token.spacing.medium

                            MD.SearchBar {
                                id: globalSearch
                                Layout.fillWidth: true
                                Layout.maximumWidth: 720
                                Layout.alignment: Qt.AlignHCenter
                                onAccepted: {
                                    root.goToPage(1)
                                    if (!root.catalogSourceId.length)
                                        root.catalogSourceId = Core.sources.firstEnabledId
                                    if (root.catalogSourceId.length)
                                        Core.searchCatalog(root.catalogSourceId, searchText)
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

                    PageNavigator {
                        id: pageStack
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        initialItem: mainPagesComponent
                    }
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
