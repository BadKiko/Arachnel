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

    function clampDiscoveryScroll() {
        if (!discoveryFlick.visible)
            return
        // Nested / interrupted transitions can leave contentY past the top and
        // clip the discovery hero under the pane edge.
        discoveryFlick.returnToBounds()
        const maxY = Math.max(0, discoveryFlick.contentHeight - discoveryFlick.height)
        discoveryFlick.contentY = Math.min(Math.max(0, discoveryFlick.contentY), maxY)
    }

    function fixViewport() {
        clampDiscoveryScroll()
        Qt.callLater(clampDiscoveryScroll)
    }

    function resetScroll() {
        catalogScrollViews.gridContentY = 0
        catalogScrollViews.listContentY = 0
        discoveryFlick.contentY = 0
    }

    function restoreScroll(listMode, value) {
        Qt.callLater(function() { Qt.callLater(function() {
            if (page.discoveryMode) {
                discoveryFlick.contentY = value
                content.clampDiscoveryScroll()
            } else if (listMode) {
                catalogScrollViews.listContentY = value
            } else {
                catalogScrollViews.gridContentY = value
            }
        }) })
    }

    function pickRandomFromShelves() {
        const shelves = [
            Core.catalogDiscovery.onFireShelf,
            Core.catalogDiscovery.friendsShelf,
            Core.catalogDiscovery.newShelf
        ]
        const pool = []
        for (let s = 0; s < shelves.length; ++s) {
            const shelf = shelves[s]
            if (!shelf || shelf.count <= 0)
                continue
            for (let i = 0; i < shelf.count; ++i)
                pool.push(shelf.entryInfo(i))
        }
        if (!pool.length)
            return
        const pick = pool[Math.floor(Math.random() * pool.length)]
        if (pick && pick.entryId)
            page.openGame(pick.entryId)
    }

    readonly property bool discoveryShelvesEmpty:
        Core.catalogDiscovery.onFireShelf.count === 0
        && Core.catalogDiscovery.friendsShelf.count === 0
        && Core.catalogDiscovery.newShelf.count === 0
    readonly property bool discoveryShowSkeleton: Core.catalogDiscovery.loading
                                                  || (Core.catalogLoading && discoveryShelvesEmpty)
    readonly property bool discoveryNoFeed: !Core.catalogDiscovery.feedLoaded
                                            && !Core.catalogDiscovery.loading
                                            && !Core.catalogLoading
    readonly property bool discoveryEmptyShelves: Core.catalogDiscovery.feedLoaded
                                                  && discoveryShelvesEmpty
                                                  && !Core.catalogDiscovery.loading
                                                  && !Core.catalogLoading

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
            visible: !page.discoveryMode
            height: visible ? implicitHeight : 0
            page: content.page
            pageMargin: page.pageMargin
        }

        Item {
            id: catalogScrollClip
            anchors.top: page.discoveryMode ? parent.top : catalogStickyToolbar.bottom
            anchors.topMargin: page.discoveryMode ? MD.Token.spacing.large : MD.Token.spacing.small
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true

            // Single ListView (not Flickable wrapping a non-interactive ListView).
            // The nested setup left contentY stuck after game-details push/pop and
            // clipped the hero under the top edge.
            ListView {
                id: discoveryFlick
                anchors.fill: parent
                visible: page.discoveryMode
                clip: true
                spacing: MD.Token.spacing.extra_large
                leftMargin: page.pageMargin
                rightMargin: page.pageMargin
                topMargin: MD.Token.spacing.medium
                bottomMargin: MD.Token.spacing.extra_large * 2
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true
                pixelAligned: true

                readonly property bool showDiscoveryShelves: Core.catalogDiscovery.feedLoaded
                                                             && (Core.catalogDiscovery.onFireShelf.count > 0
                                                                 || Core.catalogDiscovery.newShelf.count > 0
                                                                 || Core.catalogDiscovery.friendsShelf.count > 0)

                readonly property var discoveryShelfSections: [
                    {
                        title: qsTr("Recommended for you"),
                        subtitle: "",
                        iconName: MD.Token.icon.local_fire_department,
                        shelfModel: Core.catalogDiscovery.onFireShelf
                    },
                    {
                        title: qsTr("With friends"),
                        subtitle: "",
                        iconName: MD.Token.icon.groups,
                        shelfModel: Core.catalogDiscovery.friendsShelf
                    },
                    {
                        title: qsTr("Popular this week"),
                        subtitle: "",
                        iconName: MD.Token.icon.fiber_new,
                        shelfModel: Core.catalogDiscovery.newShelf
                    }
                ]

                readonly property real contentInnerWidth: width - leftMargin - rightMargin

                header: Column {
                    width: discoveryFlick.contentInnerWidth
                    spacing: MD.Token.spacing.large

                    CatalogDiscoveryHero {
                        width: parent.width
                        visible: !content.discoveryShowSkeleton && !content.discoveryNoFeed
                                 && Core.catalogDiscovery.onFireShelf.count > 0
                        page: content.page
                        shelfModel: Core.catalogDiscovery.onFireShelf
                        onOpenGame: function (id) { page.openGame(id) }
                    }

                    CatalogDiscoverySkeleton {
                        width: parent.width
                        visible: content.discoveryShowSkeleton
                        page: content.page
                    }

                    CatalogDiscoveryHeader {
                        width: parent.width
                        page: content.page
                        onBrowseAllRequested: page.openFullCatalog("")
                        onSurpriseRequested: content.pickRandomFromShelves()
                    }

                    Column {
                        width: parent.width
                        visible: content.discoveryNoFeed
                        spacing: MD.Token.spacing.small

                        MD.Label {
                            width: parent.width
                            text: qsTr("Couldn't load discovery")
                            typescale: MD.Token.typescale.title_medium
                        }

                        MD.Label {
                            width: parent.width
                            text: qsTr("Check your connection, or open All games.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        MD.Button {
                            mdState.type: MD.Enum.BtFilledTonal
                            text: qsTr("All games")
                            onClicked: page.openFullCatalog("")
                        }
                    }

                    Column {
                        width: parent.width
                        visible: content.discoveryEmptyShelves
                        spacing: MD.Token.spacing.small

                        MD.Label {
                            width: parent.width
                            text: qsTr("No matching games in your catalog")
                            typescale: MD.Token.typescale.title_medium
                        }

                        MD.Label {
                            width: parent.width
                            text: qsTr("Discovery loaded, but none of these titles are in your enabled sources yet.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }
                    }
                }

                footer: Item {
                    width: discoveryFlick.contentInnerWidth
                    height: MD.Token.spacing.extra_large * 2
                }

                model: showDiscoveryShelves ? discoveryShelfSections : []

                delegate: CatalogShelfRow {
                    required property var modelData
                    width: discoveryFlick.contentInnerWidth
                    visible: modelData.shelfModel && modelData.shelfModel.count > 0
                    height: visible ? implicitHeight : 0
                    title: modelData.title
                    subtitle: modelData.subtitle
                    iconName: modelData.iconName
                    shelfModel: modelData.shelfModel
                    page: content.page
                    onSurpriseFromShelf: function (entryId) { page.openGame(entryId) }
                }

                ScrollBar.vertical: MD.ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                onContentHeightChanged: Qt.callLater(content.clampDiscoveryScroll)
                onHeightChanged: Qt.callLater(content.clampDiscoveryScroll)
                onVisibleChanged: {
                    if (visible)
                        Qt.callLater(content.fixViewport)
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
}
