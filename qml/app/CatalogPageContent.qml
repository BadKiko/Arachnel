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

                        CatalogIntroHeader {
                            width: parent.width
                        }

                        CatalogDiscoveryHeader {
                            width: parent.width
                            page: content.page
                            onBrowseAllRequested: page.browseAllMode = true
                            onWhatToPlayRequested: page.openPlayPicker()
                        }
                    }

                    readonly property bool showDiscoveryShelves: Core.catalogDiscovery.feedLoaded
                                                                 && (Core.catalogDiscovery.onFireShelf.count > 0
                                                                     || Core.catalogDiscovery.newShelf.count > 0
                                                                     || Core.catalogDiscovery.friendsShelf.count > 0
                                                                     || Core.catalogDiscovery.soloShelf.count > 0)

                    readonly property var discoveryShelfSections: Core.catalogDiscovery.moodActive
                        ? [{
                            title: qsTr("Matches your filter"),
                            subtitle: qsTr("Sorted by buzz and freshness"),
                            iconName: MD.Token.icon.filter_alt,
                            shelfModel: Core.catalogDiscovery.onFireShelf
                        }]
                        : [
                            {
                                title: qsTr("Hits"),
                                subtitle: qsTr("Trending right now"),
                                iconName: MD.Token.icon.local_fire_department,
                                shelfModel: Core.catalogDiscovery.onFireShelf
                            },
                            {
                                title: qsTr("New games"),
                                subtitle: qsTr("Recently added to your sources"),
                                iconName: MD.Token.icon.fiber_new,
                                shelfModel: Core.catalogDiscovery.newShelf
                            },
                            {
                                title: qsTr("With friends"),
                                subtitle: qsTr("Co-op and multiplayer"),
                                iconName: MD.Token.icon.groups,
                                shelfModel: Core.catalogDiscovery.friendsShelf
                            },
                            {
                                title: qsTr("Solo"),
                                subtitle: qsTr("Single-player adventures"),
                                iconName: MD.Token.icon.person,
                                shelfModel: Core.catalogDiscovery.soloShelf
                            }
                        ]

                    model: showDiscoveryShelves ? discoveryShelfSections : []

                    delegate: CatalogShelfRow {
                        required property var modelData
                        width: discoveryList.width - discoveryList.leftMargin - discoveryList.rightMargin
                        title: modelData.title
                        subtitle: modelData.subtitle
                        iconName: modelData.iconName
                        shelfModel: modelData.shelfModel
                        page: content.page
                    }

                    footer: MD.Button {
                        width: discoveryList.width - discoveryList.leftMargin - discoveryList.rightMargin
                        visible: showDiscoveryShelves
                        mdState.type: MD.Enum.BtOutlined
                        text: qsTr("Browse all games")
                        onClicked: page.browseAllMode = true
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
