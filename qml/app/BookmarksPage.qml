import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int pageMargin: Appearance.pageMargin
    readonly property int gridSpacing: MD.Token.spacing.medium
    readonly property int minCardWidth: 160
    readonly property int metaHeight: 48
    readonly property bool favoritesEmpty: favoritesModel.count === 0

    signal openGame(string gameId)
    signal openCatalog()

    ListModel {
        id: favoritesModel
    }

    property bool refreshing: false

    function refreshFavorites() {
        if (root.refreshing)
            return
        root.refreshing = true
        const rows = Core.settings.bookmarks || []
        favoritesModel.clear()
        const snapshots = []
        for (let i = 0; i < rows.length; ++i) {
            const row = rows[i] || {}
            const id = String(row.gameId || row.entryId || "").trim()
            if (!id.length)
                continue

            const info = Core.entryDetails(id)
            const liveTitle = String(info.title || "").trim()
            const liveCover = String(info.coverUrl || "").trim()
            const liveSource = String(info.sourceName || info.sourceId || "").trim()
            const title = liveTitle.length ? liveTitle : String(row.title || "")
            const coverUrl = liveCover.length ? liveCover : String(row.coverUrl || "")
            const sourceName = liveSource.length ? liveSource : String(row.sourceName || "")

            favoritesModel.append({
                                      gameId: id,
                                      title: title,
                                      coverUrl: coverUrl,
                                      sourceName: sourceName
                                  })

            if (liveTitle.length || liveCover.startsWith("file:") || liveSource.length)
                snapshots.push({
                                   id: id,
                                   title: liveTitle,
                                   coverUrl: liveCover,
                                   sourceName: liveSource
                               })
        }
        root.refreshing = false
        for (let s = 0; s < snapshots.length; ++s) {
            const snap = snapshots[s]
            Core.settings.upsertBookmarkSnapshot(snap.id, snap.title, snap.coverUrl, snap.sourceName)
        }
    }

    Connections {
        target: Core.settings
        function onBookmarkedEntryIdsChanged() { root.refreshFavorites() }
    }

    Connections {
        target: Core
        function onPluginsChanged() { root.refreshFavorites() }
        function onCatalogStatusChanged() { root.refreshFavorites() }
        function onEntryMetadataChanged(entryId) {
            for (let i = 0; i < favoritesModel.count; ++i) {
                if (favoritesModel.get(i).gameId === entryId) {
                    root.refreshFavorites()
                    return
                }
            }
        }
    }

    Connections {
        target: Core.library
        function onLibraryChanged() { root.refreshFavorites() }
    }

    Component.onCompleted: refreshFavorites()

    EmptyState {
        shown: root.favoritesEmpty
        title: qsTr("No favorites")
        message: Messages.favoritesEmptyHint
        actionText: qsTr("Open catalog")
        onActionTriggered: root.openCatalog()
    }

    Flickable {
        anchors.fill: parent
        opacity: !root.favoritesEmpty ? 1 : 0
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation {
                duration: AppMotion.short
                easing: AppMotion.standard
            }
        }
        contentWidth: width
        contentHeight: contentCol.implicitHeight + pageMargin
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: parent.width
            spacing: MD.Token.spacing.medium

            PageHeader {
                title: qsTr("Favorites")
                subtitle: qsTr("%n game(s)", "", favoritesModel.count)
            }

            Item {
                id: gridHost
                Layout.fillWidth: true
                Layout.leftMargin: pageMargin
                Layout.rightMargin: pageMargin

                readonly property int gap: root.gridSpacing
                readonly property int columns: {
                    if (width <= 0)
                        return 2
                    return Math.max(2, Math.floor((width + gap) / (root.minCardWidth + gap)))
                }
                readonly property int cellW: columns > 0 ? Math.floor(width / columns) : root.minCardWidth + gap
                readonly property int cardWidth: Math.max(1, cellW - gap)
                readonly property int cardHeight: Math.ceil(cardWidth * 4 / 3) + root.metaHeight
                readonly property int cellH: cardHeight + gap
                readonly property int rows: Math.max(
                    1, Math.ceil(favoritesModel.count / Math.max(1, columns)))
                Layout.preferredHeight: rows * cellH

                GridView {
                    width: gridHost.cellW * gridHost.columns
                    height: parent.height
                    clip: false
                    interactive: false
                    model: favoritesModel
                    cellWidth: gridHost.cellW
                    cellHeight: gridHost.cellH
                    cacheBuffer: 0
                    populate: StaggerEnterTransition {}
                    add: StaggerEnterTransition { staggered: false }

                    delegate: FavoriteGameCard {
                        width: gridHost.cardWidth
                        height: gridHost.cardHeight
                        onOpenDetails: function (id) { root.openGame(id) }
                    }
                }
            }
        }
    }
}
