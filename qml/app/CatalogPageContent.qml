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

    function openShortInstallCatalog() {
        // Size band 2 = 1–5 GB in catalog filters (closest “short install” preset).
        Core.applyCatalogPresentation(Core.catalog.sortMode, -1, 2, 0, false, "", 0)
        page.openFullCatalog("")
    }

    function showFriendsMood() {
        Core.catalogDiscovery.moodId = "friends"
        discoveryFlick.contentY = 0
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
            anchors.topMargin: page.discoveryMode ? MD.Token.spacing.medium : MD.Token.spacing.small
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
                    spacing: MD.Token.spacing.extra_large
                    leftMargin: page.pageMargin
                    rightMargin: page.pageMargin
                    topMargin: MD.Token.spacing.small
                    bottomMargin: MD.Token.spacing.extra_large

                    header: Column {
                        width: discoveryList.width - discoveryList.leftMargin - discoveryList.rightMargin
                        spacing: MD.Token.spacing.large

                        CatalogDiscoveryHero {
                            width: parent.width
                            visible: !content.discoveryShowSkeleton && !content.discoveryNoFeed
                                     && Core.catalogDiscovery.onFireShelf.count > 0
                                     && !Core.catalogDiscovery.moodActive
                            page: content.page
                            shelfModel: Core.catalogDiscovery.onFireShelf
                            onOpenGame: function (id) { page.openGame(id) }
                            onSurpriseRequested: content.pickRandomFromShelves()
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
                            onHelpMePickRequested: content.openPlayPicker()
                            onSurpriseRequested: content.pickRandomFromShelves()
                            onShortSessionRequested: content.openShortInstallCatalog()
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

                    readonly property bool showDiscoveryShelves: Core.catalogDiscovery.feedLoaded
                                                                 && (Core.catalogDiscovery.onFireShelf.count > 0
                                                                     || Core.catalogDiscovery.newShelf.count > 0
                                                                     || Core.catalogDiscovery.friendsShelf.count > 0)

                    readonly property var discoveryShelfSections: {
                        if (Core.catalogDiscovery.moodActive) {
                            return [{
                                title: qsTr("Matches your mood"),
                                subtitle: qsTr("Tap a cover to open details"),
                                iconName: MD.Token.icon.auto_awesome,
                                shelfModel: Core.catalogDiscovery.onFireShelf
                            }]
                        }
                        return [
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
                    }

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
                        onSurpriseFromShelf: function (entryId) { page.openGame(entryId) }
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
            page.persistCatalogFilters()
            if (page.browseOnly)
                page.browseAllMode = true
            else
                page.openFullCatalog("")
        }
    }

    function openPlayPicker() {
        playPickerSheet.openSheet()
    }
}
