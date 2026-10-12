import QtQuick
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: content

    required property var page
    required property var prefs

    property alias searchText: catalogStickyToolbar.searchText

    function focusSearch() {
        catalogStickyToolbar.focusSearch()
    }
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
        catalogScrollViews.scrollToTop()
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
            anchors.topMargin: page.discoveryMode ? page.pageMargin
                                                 : MD.Token.spacing.small
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true

            // Plain Flickable + Column (not ListView). ListView + JS-array model
            // dropped shelf rows after tab/stack transitions; nested non-interactive
            // ListView previously left contentY stuck. Three shelves don't need
            // virtualization - only the horizontal carousels inside each row do.
            DiscoverFeed {
                id: discoverFeed
                anchors.fill: parent
                anchors.leftMargin: page.pageMargin
                anchors.rightMargin: page.pageMargin
                anchors.bottomMargin: page.pageMargin
                visible: page.discoveryMode
                page: content.page
                onOpenGame: function (id) { content.page.openGame(id) }
                onVisibleChanged: if (visible) forceActiveFocus()
            }

            Flickable {
                id: discoveryFlick
                anchors.fill: parent
                visible: false
                clip: true
                contentWidth: width
                contentHeight: discoveryColumn.height
                boundsBehavior: Flickable.StopAtBounds
                pixelAligned: true

                // v2: Discover is the feed below; the old shelves are gone.
                Item { id: discoveryColumn; height: 0 }

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

            CatalogLoadingState {
                anchors.fill: parent
                z: 1
                visible: !page.discoveryMode && Core.catalogLoading && Core.catalog.count === 0
                         && !Core.catalogPartial
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
