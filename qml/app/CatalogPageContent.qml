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
        // Stack / tab transitions can leave contentY past the top and clip the
        // discovery hero under the pane edge.
        const maxY = Math.max(0, discoveryFlick.contentHeight - discoveryFlick.height)
        if (!Number.isFinite(discoveryFlick.contentY) || discoveryFlick.contentY < 0)
            discoveryFlick.contentY = 0
        else if (discoveryFlick.contentY > maxY)
            discoveryFlick.contentY = maxY
        discoveryFlick.returnToBounds()
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
        catalogScrollViews.restoreContentY(value)
        Qt.callLater(function () {
            catalogScrollViews.restoreContentY(value)
            Qt.callLater(function () { catalogScrollViews.restoreContentY(value) })
        })
    }

    function firstVisibleRow() {
        const view = catalogScrollViews.activeView()
        if (!view)
            return 0
        return catalogScrollViews.firstVisibleRow(view)
    }

    function restoreBrowsePlace(entryId, row, contentY, listMode) {
        let attempt = 0
        function apply() {
            if (page.discoveryMode) {
                discoveryFlick.contentY = contentY || 0
                content.clampDiscoveryScroll()
                return true
            }
            // Exact viewport first. Do NOT fall back to jumpToRow while contentY
            // is still pending layout - that pins the game to the top of the screen.
            if (contentY > 0) {
                if (catalogScrollViews.restoreContentY(contentY))
                    return true
                if (attempt < 20)
                    return false
            }
            if (entryId && entryId.length) {
                const idx = Core.catalog.indexOfEntry(entryId)
                if (idx >= 0 && catalogScrollViews.jumpToRow(idx, false))
                    return true
            }
            if (row >= 0 && catalogScrollViews.jumpToRow(row, false))
                return true
            return !(contentY > 0 || (entryId && entryId.length) || row >= 0)
        }
        function tick() {
            if (apply())
                return
            if (++attempt > 24)
                return
            Qt.callLater(tick)
        }
        tick()
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
            anchors.topMargin: page.discoveryMode ? MD.Token.spacing.extra_large
                                                 : MD.Token.spacing.small
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true

            // Plain Flickable + Column (not ListView). ListView + JS-array model
            // dropped shelf rows after tab/stack transitions; nested non-interactive
            // ListView previously left contentY stuck. Three shelves don't need
            // virtualization - only the horizontal carousels inside each row do.
            Flickable {
                id: discoveryFlick
                anchors.fill: parent
                visible: page.discoveryMode
                clip: true
                contentWidth: width
                contentHeight: discoveryColumn.height
                boundsBehavior: Flickable.StopAtBounds
                pixelAligned: true

                Column {
                    id: discoveryColumn
                    width: discoveryFlick.width - page.pageMargin * 2
                    x: page.pageMargin
                    topPadding: MD.Token.spacing.large
                    bottomPadding: MD.Token.spacing.extra_large * 2
                    spacing: MD.Token.spacing.extra_large

                    Column {
                        width: parent.width
                        spacing: MD.Token.spacing.large

                        CatalogDiscoveryHero {
                            width: parent.width
                            visible: !content.discoveryShowSkeleton && !content.discoveryNoFeed
                                     && Core.catalogDiscovery.onFireShelf.count > 0
                            height: visible ? implicitHeight : 0
                            page: content.page
                            shelfModel: Core.catalogDiscovery.onFireShelf
                            onOpenGame: function (id) { page.openGame(id) }
                        }

                        CatalogDiscoverySkeleton {
                            width: parent.width
                            visible: content.discoveryShowSkeleton
                            height: visible ? implicitHeight : 0
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
                            height: visible ? implicitHeight : 0
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
                            height: visible ? implicitHeight : 0
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

                    CatalogShelfRow {
                        width: parent.width
                        visible: Core.catalogDiscovery.feedLoaded
                                 && Core.catalogDiscovery.onFireShelf.count > 0
                        height: visible ? implicitHeight : 0
                        title: qsTr("Recommended for you")
                        subtitle: ""
                        iconName: MD.Token.icon.local_fire_department
                        shelfModel: Core.catalogDiscovery.onFireShelf
                        page: content.page
                        onSurpriseFromShelf: function (entryId) { page.openGame(entryId) }
                    }

                    CatalogShelfRow {
                        width: parent.width
                        visible: Core.catalogDiscovery.feedLoaded
                                 && Core.catalogDiscovery.friendsShelf.count > 0
                        height: visible ? implicitHeight : 0
                        title: qsTr("With friends")
                        subtitle: ""
                        iconName: MD.Token.icon.groups
                        shelfModel: Core.catalogDiscovery.friendsShelf
                        page: content.page
                        onSurpriseFromShelf: function (entryId) { page.openGame(entryId) }
                    }

                    CatalogShelfRow {
                        width: parent.width
                        visible: Core.catalogDiscovery.feedLoaded
                                 && Core.catalogDiscovery.newShelf.count > 0
                        height: visible ? implicitHeight : 0
                        title: qsTr("Popular this week")
                        subtitle: ""
                        iconName: MD.Token.icon.fiber_new
                        shelfModel: Core.catalogDiscovery.newShelf
                        page: content.page
                        onSurpriseFromShelf: function (entryId) { page.openGame(entryId) }
                    }
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
            // Leave the alphabet scrubber uncovered on the right.
            anchors.rightMargin: catalogScrollViews.scrubberWidth
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
