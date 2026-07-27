import QtQuick
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: content

    required property var page
    required property var prefs

    property alias searchText: catalogStickyToolbar.searchText
    property alias gridContentY: catalogScrollViews.gridContentY
    property alias listContentY: catalogScrollViews.listContentY

    readonly property real currentContentY: page.discoveryMode
                                            ? discoveryFlick.contentY
                                            : (page.listViewMode
                                               ? catalogScrollViews.listContentY
                                               : catalogScrollViews.gridContentY)

    function resetScroll() {
        catalogScrollViews.gridContentY = 0
        catalogScrollViews.listContentY = 0
        discoveryFlick.contentY = 0
    }

    function restoreScroll(listMode, value) {
        Qt.callLater(function() { Qt.callLater(function() {
            if (page.discoveryMode)
                discoveryFlick.contentY = value
            else if (listMode)
                catalogScrollViews.listContentY = value
            else
                catalogScrollViews.gridContentY = value
        }) })
    }

    Loader {
        id: catalogIntroHeightProbe
        visible: false
        width: Math.max(0, page.width - page.pageMargin * 2)
        sourceComponent: CatalogIntroHeader {}
    }

    readonly property real catalogIntroHeaderHeight: catalogIntroHeightProbe.item
                                                     ? catalogIntroHeightProbe.item.implicitHeight
                                                     : 160

    Item {
        anchors.fill: parent
        visible: !page.noSources

        CatalogStickyToolbar {
            id: catalogStickyToolbar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: page.pageMargin
            anchors.rightMargin: page.pageMargin
            anchors.topMargin: MD.Token.spacing.medium
            z: 4
            layer.enabled: true
            page: content.page
            pageMargin: page.pageMargin
        }

        Item {
            id: catalogScrollClip
            anchors.top: catalogStickyToolbar.bottom
            anchors.topMargin: MD.Token.spacing.small
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true

            Flickable {
                id: discoveryFlick
                anchors.fill: parent
                visible: page.discoveryMode
                contentWidth: width
                contentHeight: discoveryList.contentHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ListView {
                    id: discoveryList
                    width: discoveryFlick.width
                    height: contentHeight
                    interactive: false
                    spacing: MD.Token.spacing.large
                    leftMargin: page.pageMargin
                    rightMargin: page.pageMargin
                    topMargin: MD.Token.spacing.small
                    bottomMargin: MD.Token.spacing.large

                    header: Column {
                        width: discoveryList.width - discoveryList.leftMargin - discoveryList.rightMargin
                        spacing: MD.Token.spacing.small

                        CatalogDiscoveryHeader {
                            width: parent.width
                            page: content.page
                            onBrowseAllRequested: page.browseAllMode = true
                        }
                    }

                    readonly property bool showDiscoveryShelves: Core.catalogDiscovery.feedLoaded
                                                                 && (Core.catalogDiscovery.onFireShelf.count > 0
                                                                     || Core.catalogDiscovery.newShelf.count > 0
                                                                     || Core.catalogDiscovery.friendsShelf.count > 0
                                                                     || Core.catalogDiscovery.soloShelf.count > 0)

                    readonly property var discoveryShelfSections: [
                        {
                            title: qsTr("Hits"),
                            subtitle: qsTr("What's buzzing right now"),
                            iconName: MD.Token.icon.local_fire_department,
                            shelfModel: Core.catalogDiscovery.onFireShelf
                        },
                        {
                            title: qsTr("With friends"),
                            subtitle: qsTr("Chaotic co-op and party games"),
                            iconName: MD.Token.icon.groups,
                            shelfModel: Core.catalogDiscovery.friendsShelf
                        },
                        {
                            title: qsTr("Solo"),
                            subtitle: qsTr("Single-player picks worth trying"),
                            iconName: MD.Token.icon.person,
                            shelfModel: Core.catalogDiscovery.soloShelf
                        },
                        {
                            title: qsTr("New games"),
                            subtitle: qsTr("Fresh releases"),
                            iconName: MD.Token.icon.fiber_new,
                            shelfModel: Core.catalogDiscovery.newShelf
                        }
                    ]

                    model: showDiscoveryShelves ? discoveryShelfSections : []

                    delegate: CatalogShelfRow {
                        required property var modelData
                        width: discoveryList.width - discoveryList.leftMargin - discoveryList.rightMargin
                        visible: modelData.shelfModel && modelData.shelfModel.count > 0
                        height: visible ? implicitHeight : 0
                        title: modelData.title
                        subtitle: modelData.subtitle
                        iconName: modelData.iconName
                        shelfModel: modelData.shelfModel
                        page: content.page
                    }

                    footer: Item {
                        width: discoveryList.width - discoveryList.leftMargin - discoveryList.rightMargin
                        height: showDiscoveryShelves ? browseAllFooter.implicitHeight + MD.Token.spacing.medium : 0

                        MD.Button {
                            id: browseAllFooter
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            visible: showDiscoveryShelves
                            mdState.type: MD.Enum.BtOutlined
                            text: qsTr("All games")
                            onClicked: page.browseAllMode = true
                        }
                    }
                }

                ScrollBar.vertical: MD.ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            CatalogScrollViews {
                id: catalogScrollViews
                anchors.fill: parent
                visible: !page.discoveryMode
                page: content.page
                prefs: content.prefs
                pageMargin: page.pageMargin
            }
        }

        CatalogCompactBar {
            id: compactBar
            anchors.top: catalogStickyToolbar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            z: 5
            visible: !page.discoveryMode
            page: content.page
            prefs: content.prefs
            pageMargin: page.pageMargin
            barOpacity: page.compactBarOpacity
        }

        Item {
            anchors.fill: parent
            anchors.topMargin: 200
            visible: page.catalogEmpty && !Core.catalogLoading && !page.discoveryMode
            z: 1

            CatalogEmptyResults {
                anchors.fill: parent
                page: content.page
                pageMargin: page.pageMargin
            }
        }
    }

    CatalogNoSourcesPanel {
        anchors.fill: parent
        visible: page.noSources
        page: content.page
        pageMargin: page.pageMargin
    }

    CatalogPlayPickerSheet {
        id: playPickerSheet
        onApplied: {
            page.browseAllMode = true
            page.persistCatalogFilters()
        }
    }

    function openPlayPicker() {
        playPickerSheet.openSheet()
    }
}
