import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int pageMargin: MD.Token.spacing.large
    readonly property int cardRadius: MD.Token.shape.corner.extra_large
    readonly property int thumbSize: 56
    readonly property int mapCardWidth: 180

    property string selectedGameId: ""
    property string selectedGameTitle: ""
    property string selectedSteamAppId: ""
    property int libraryRevision: 0
    property int workshopPage: 1
    property bool workshopHasMore: false
    property bool workshopLoading: false
    property string workshopError: ""
    property var workshopMaps: []

    readonly property bool gameSelected: selectedGameId.length > 0
    readonly property var workshopGames: {
        void libraryRevision
        const out = []
        const n = Core.library.count
        for (let i = 0; i < n; ++i) {
            const g = Core.library.gameAt(i)
            const appId = String(g.steamAppId || "")
            if (!appId.length)
                continue
            const installPath = String(g.installPath || "")
            out.push({
                         gameId: g.gameId || "",
                         title: g.title || "",
                         coverUrl: g.coverUrl || "",
                         steamAppId: appId,
                         workshopSupported: true,
                         gameInstalled: installPath.length > 0
                     })
        }
        return out
    }
    readonly property bool hasWorkshopGames: workshopGames.length > 0

    function clearSelection() {
        selectedGameId = ""
        selectedGameTitle = ""
        selectedSteamAppId = ""
        workshopMaps = []
        workshopPage = 1
        workshopHasMore = false
        workshopLoading = false
        workshopError = ""
    }

    function selectGame(game) {
        if (!game || !game.workshopSupported)
            return
        selectedGameId = game.gameId
        selectedGameTitle = game.title
        selectedSteamAppId = game.steamAppId
        workshopMaps = []
        workshopPage = 1
        workshopHasMore = false
        workshopError = ""
        workshopLoading = true
        Core.requestWorkshopPage(selectedSteamAppId, 1)
    }

    function loadMoreMaps() {
        if (!selectedSteamAppId.length || workshopLoading || !workshopHasMore)
            return
        workshopLoading = true
        Core.requestWorkshopPage(selectedSteamAppId, workshopPage + 1)
    }

    function mapPreview(item) {
        const local = item.localPreviewUrl || Core.workshopPreviewUrl(item.previewUrl || "")
        if (local && String(local).startsWith("file:"))
            return local
        return ""
    }

    function mapStatus(publishedFileId) {
        void libraryRevision
        if (!selectedGameId.length || !publishedFileId)
            return ""
        return Core.workshopItemStatus(selectedGameId, String(publishedFileId)) || ""
    }

    function isMapInstalled(publishedFileId) {
        return root.mapStatus(publishedFileId) === "installed"
    }

    function isMapCached(publishedFileId) {
        return root.mapStatus(publishedFileId) === "cached"
    }

    function selectedGameInstalled() {
        void libraryRevision
        const info = Core.library.gameInfo(selectedGameId)
        return !!(info && info.installPath)
    }

    Connections {
        target: Core.library
        function onCountChanged() { root.libraryRevision++ }
        function onLibraryChanged() { root.libraryRevision++ }
    }

    Connections {
        target: Core
        function onWorkshopPageReady(steamAppId, page, items, hasMore) {
            if (steamAppId !== root.selectedSteamAppId)
                return
            root.workshopLoading = false
            root.workshopError = ""
            root.workshopPage = page
            root.workshopHasMore = !!hasMore
            const next = page <= 1 ? [] : (root.workshopMaps.slice() || [])
            for (let i = 0; i < items.length; ++i) {
                const item = items[i]
                let found = false
                for (let j = 0; j < next.length; ++j) {
                    if (next[j].publishedFileId === item.publishedFileId) {
                        next[j] = item
                        found = true
                        break
                    }
                }
                if (!found)
                    next.push(item)
                if (item.previewUrl)
                    Core.requestWorkshopPreview(item.previewUrl)
            }
            root.workshopMaps = next
        }
        function onWorkshopPageFailed(steamAppId, page, error) {
            if (steamAppId !== root.selectedSteamAppId)
                return
            root.workshopLoading = false
            root.workshopError = error || qsTr("Could not load Workshop")
        }
        function onWorkshopItemUpdated(steamAppId, item) {
            if (steamAppId !== root.selectedSteamAppId || !item || !item.publishedFileId)
                return
            const maps = root.workshopMaps.slice()
            for (let i = 0; i < maps.length; ++i) {
                if (maps[i].publishedFileId === item.publishedFileId) {
                    maps[i] = item
                    root.workshopMaps = maps
                    break
                }
            }
            if (itemSheet.opened
                    && itemSheet.item
                    && itemSheet.item.publishedFileId === item.publishedFileId) {
                itemSheet.item = item
            }
            const urls = item.previewUrls
            if (urls && urls.length) {
                for (let j = 0; j < Math.min(6, urls.length); ++j) {
                    if (urls[j])
                        Core.requestWorkshopPreview(String(urls[j]))
                }
            }
        }
        function onWorkshopPreviewReady(previewUrl, localUrl) {
            const maps = root.workshopMaps.slice()
            let changed = false
            for (let i = 0; i < maps.length; ++i) {
                if (maps[i].previewUrl === previewUrl) {
                    maps[i] = Object.assign({}, maps[i], { localPreviewUrl: localUrl })
                    changed = true
                }
            }
            if (changed)
                root.workshopMaps = maps
        }
    }

    // ── Pick game ────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: MD.Token.spacing.medium
        visible: !root.gameSelected

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
            Layout.topMargin: pageMargin
            spacing: MD.Token.spacing.extra_small

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Workshop")
                typescale: MD.Token.typescale.headline_small
            }

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Choose a library game to browse Workshop files.")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WordWrap
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.hasWorkshopGames

            ColumnLayout {
                anchors.centerIn: parent
                spacing: MD.Token.spacing.medium
                width: Math.min(parent.width - pageMargin * 2, 420)

                SpiderWebMark {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 140
                    Layout.preferredHeight: 140
                    width: 140
                    height: 140
                    strokeColor: MD.Token.color.primary
                    strokeWidth: 2.5
                    opacity: 0.35
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("No library games with Steam App ID")
                    typescale: MD.Token.typescale.title_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Add a Steam game to your library first.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                    wrapMode: Text.WordWrap
                }
            }
        }

        Flickable {
            id: gamesFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.hasWorkshopGames
            clip: true
            contentWidth: width
            contentHeight: gamesColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: gamesColumn
                width: gamesFlick.width
                spacing: MD.Token.spacing.small

                Repeater {
                    model: root.workshopGames

                    Rectangle {
                        id: gameRow
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.leftMargin: pageMargin
                        Layout.rightMargin: pageMargin
                        radius: root.cardRadius
                        color: MD.Token.color.surface_container
                        opacity: modelData.workshopSupported ? 1 : 0.55
                        implicitHeight: row.implicitHeight + MD.Token.spacing.medium * 2

                        readonly property bool supported: !!modelData.workshopSupported

                        MouseArea {
                            anchors.fill: parent
                            enabled: gameRow.supported
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectGame(gameRow.modelData)
                        }

                        RowLayout {
                            id: row
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: MD.Token.spacing.medium
                            spacing: MD.Token.spacing.medium

                            Item {
                                Layout.preferredWidth: root.thumbSize
                                Layout.preferredHeight: root.thumbSize
                                Layout.maximumWidth: root.thumbSize
                                Layout.maximumHeight: root.thumbSize
                                clip: true

                                layer.enabled: true
                                layer.effect: MD.RoundClip {
                                    corners: MD.Util.corners(MD.Token.shape.corner.large)
                                    size: Qt.vector2d(root.thumbSize, root.thumbSize)
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    color: MD.Token.color.surface_container_high
                                }

                                Image {
                                    anchors.fill: parent
                                    source: {
                                        const s = String(gameRow.modelData.coverUrl || "")
                                        return s.startsWith("file:") ? s : ""
                                    }
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: true
                                    sourceSize.width: root.thumbSize * 2
                                    sourceSize.height: root.thumbSize * 2
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 2

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: gameRow.modelData.title
                                    typescale: MD.Token.typescale.title_small
                                    elide: Text.ElideRight
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: !gameRow.modelData.gameInstalled
                                          ? qsTr("Not installed - downloads go to cache")
                                          : qsTr("Workshop files")
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.label_medium
                                }
                            }

                            MD.Icon {
                                visible: gameRow.supported
                                Layout.alignment: Qt.AlignVCenter
                                name: MD.Token.icon.chevron_right
                                size: 20
                                color: MD.Token.color.on_surface_variant
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: pageMargin
                }
            }
        }
    }

    // ── Maps ─────────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: MD.Token.spacing.medium
        visible: root.gameSelected

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
            Layout.topMargin: pageMargin
            spacing: MD.Token.spacing.small

            MD.IconButton {
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.arrow_back
                onClicked: root.clearSelection()
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    Layout.fillWidth: true
                    text: root.selectedGameTitle
                    typescale: MD.Token.typescale.title_large
                    elide: Text.ElideRight
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: root.selectedGameInstalled()
                          ? qsTr("Workshop files")
                          : qsTr("Game not installed - downloads go to cache")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.workshopLoading && root.workshopMaps.length === 0

            MD.CircularIndicator {
                anchors.centerIn: parent
                indeterminate: true
                running: true
                strokeWidth: 2.5
                width: 36
                height: 36
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.workshopLoading && root.workshopError.length > 0
                     && root.workshopMaps.length === 0

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - pageMargin * 2, 420)
                spacing: MD.Token.spacing.medium

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Could not load Workshop")
                    typescale: MD.Token.typescale.title_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: root.workshopError
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                    wrapMode: Text.WordWrap
                }

                MD.Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Retry")
                    onClicked: {
                        root.workshopLoading = true
                        root.workshopError = ""
                        Core.requestWorkshopPage(root.selectedSteamAppId, 1)
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.workshopLoading && root.workshopError.length === 0
                     && root.workshopMaps.length === 0

            ColumnLayout {
                anchors.centerIn: parent
                spacing: MD.Token.spacing.medium
                width: Math.min(parent.width - pageMargin * 2, 420)

                SpiderWebMark {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 140
                    Layout.preferredHeight: 140
                    width: 140
                    height: 140
                    strokeColor: MD.Token.color.primary
                    strokeWidth: 2.5
                    opacity: 0.35
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("No files yet")
                    typescale: MD.Token.typescale.title_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Workshop files will show here.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                    wrapMode: Text.WordWrap
                }
            }
        }

        Flickable {
            id: mapsFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.workshopMaps.length > 0
            clip: true
            contentWidth: width
            contentHeight: mapsColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: mapsColumn
                width: mapsFlick.width
                spacing: MD.Token.spacing.medium

                GridLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: pageMargin
                    Layout.rightMargin: pageMargin
                    columns: Math.max(1, Math.floor((mapsFlick.width - pageMargin * 2 + MD.Token.spacing.medium)
                                                    / (root.mapCardWidth + MD.Token.spacing.medium)))
                    columnSpacing: MD.Token.spacing.medium
                    rowSpacing: MD.Token.spacing.medium

                    Repeater {
                        model: root.workshopMaps

                        Rectangle {
                            id: mapCard
                            required property var modelData

                            Layout.fillWidth: true
                            Layout.preferredWidth: root.mapCardWidth
                            Layout.maximumWidth: root.mapCardWidth + 40
                            radius: root.cardRadius
                            color: MD.Token.color.surface_container
                            implicitHeight: mapCol.implicitHeight + MD.Token.spacing.medium

                            readonly property bool shotsPending: !mapCard.modelData.screenshotsResolved
                                                                && !!(mapCard.modelData.previewUrl
                                                                      || mapCard.modelData.publishedFileId)
                            property bool shotWaiting: false

                            ColumnLayout {
                                id: mapCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: MD.Token.spacing.small
                                spacing: MD.Token.spacing.small

                                Item {
                                    id: posterHost
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: width * 0.56
                                    clip: true

                                    layer.enabled: true
                                    layer.effect: MD.RoundClip {
                                        corners: MD.Util.corners(MD.Token.shape.corner.large)
                                        size: Qt.vector2d(width, height)
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        color: MD.Token.color.surface_container_high
                                    }

                                    Image {
                                        id: posterImage
                                        anchors.fill: parent
                                        source: root.mapPreview(mapCard.modelData)
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        cache: true
                                        opacity: posterMouse.containsMouse ? 1 : 0.96
                                    }

                                    MouseArea {
                                        id: posterMouse
                                        anchors.fill: parent
                                        z: 20
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: itemSheet.openFor(root.selectedGameId,
                                                                     mapCard.modelData)
                                        onContainsMouseChanged: {
                                            if (!containsMouse) {
                                                mapCard.shotWaiting = false
                                                shotWaitTimer.stop()
                                                return
                                            }
                                            if (mapCard.shotsPending) {
                                                mapCard.shotWaiting = true
                                                shotWaitTimer.restart()
                                            }
                                            const raw = mapCard.modelData.previewUrls
                                            if (raw && raw.length) {
                                                for (let i = 0; i < Math.min(6, raw.length); ++i) {
                                                    if (raw[i])
                                                        Core.requestWorkshopPreview(String(raw[i]))
                                                }
                                            } else if ((mapCard.modelData.previewUrl || "").length) {
                                                Core.requestWorkshopPreview(mapCard.modelData.previewUrl)
                                            }
                                        }
                                    }

                                    Timer {
                                        id: shotWaitTimer
                                        interval: 12000
                                        repeat: false
                                        onTriggered: mapCard.shotWaiting = false
                                    }

                                    // Same soft wait cue as catalog game cards.
                                    MD.LinearIndicator {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        anchors.margins: MD.Token.spacing.small
                                        z: 3
                                        implicitHeight: 5
                                        strokeWidth: 3
                                        indeterminate: true
                                        wavy: true
                                        waveAmplitude: 1.8
                                        waveLength: 18
                                        visible: posterImage.status === Image.Loading
                                                 || (mapCard.shotWaiting
                                                     && posterMouse.containsMouse
                                                     && mapCard.shotsPending)
                                        running: visible
                                        color: MD.Token.color.primary
                                        trackColor: MD.Util.transparent(MD.Token.color.on_surface, 0.18)
                                    }
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: mapCard.modelData.title || ""
                                    typescale: MD.Token.typescale.title_small
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    visible: !!(mapCard.modelData.sizeLabel)
                                    text: mapCard.modelData.sizeLabel || ""
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.label_medium
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    visible: {
                                        const s = root.mapStatus(mapCard.modelData.publishedFileId)
                                        return s === "installed" || s === "cached"
                                    }
                                    text: {
                                        const s = root.mapStatus(mapCard.modelData.publishedFileId)
                                        if (s === "installed")
                                            return qsTr("Installed")
                                        if (s === "cached")
                                            return qsTr("Cached")
                                        return ""
                                    }
                                    color: MD.Token.color.primary
                                    typescale: MD.Token.typescale.label_medium
                                }
                            }
                        }
                    }
                }

                MD.Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: pageMargin
                    visible: root.workshopHasMore
                    enabled: !root.workshopLoading
                    text: root.workshopLoading ? qsTr("Loading…") : qsTr("Load more")
                    onClicked: root.loadMoreMaps()
                }
            }
        }
    }

    WorkshopItemSheet {
        id: itemSheet
        anchors.fill: parent
    }
}
