import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property string entryId
    required property string title
    required property string coverUrl
    required property string version
    required property string uploadDate
    required property string sizeLabel
    required property string installKindLabel
    required property bool metadataPending

    property bool compactRow: false
    property int currentPlayers: -1
    property bool peekArmed: false
    property bool peekWaiting: false
    /** Loaded from entryInfo only when peek arms — not a GridView model role. */
    property var peekScreenshotUrls: []
    /** Pass navRail.width so screenshot peek doesn't render over the nav rail. */
    property real peekLeftEdge: 0

    signal openDetails(string entryId)

    clip: true

    readonly property var shotUrls: {
        if (!root.peekArmed && !root.peekWaiting)
            return []
        const raw = root.peekScreenshotUrls
        if (raw === undefined || raw === null)
            return []
        const len = raw.length !== undefined ? raw.length : 0
        if (!len)
            return []
        const out = []
        for (let i = 0; i < len; ++i) {
            const u = String(raw[i] || "")
            if (u.length)
                out.push(u)
        }
        return out
    }

    onShotUrlsChanged: {
        if (root.shotUrls.length > 0) {
            root.peekWaiting = false
            peekTimeout.stop()
        }
    }

    readonly property string playersLabel: {
        if (root.currentPlayers < 0)
            return ""
        if (root.currentPlayers >= 1000000)
            return (root.currentPlayers / 1000000).toFixed(1) + "M " + qsTr("playing")
        if (root.currentPlayers >= 1000)
            return (root.currentPlayers / 1000).toFixed(1) + "K " + qsTr("playing")
        return root.currentPlayers + " " + qsTr("playing")
    }

    readonly property string metaLine: {
        const parts = []
        if (root.playersLabel.length > 0)
            parts.push(root.playersLabel)
        if ((root.version || "").length > 0)
            parts.push("v" + root.version)
        else if ((root.uploadDate || "").length >= 10)
            parts.push(root.uploadDate.substring(0, 10))
        if ((root.sizeLabel || "").length > 0)
            parts.push(root.sizeLabel)
        return parts.join(" · ")
    }

    readonly property string displayCoverUrl: coverUrl.startsWith("file:") ? coverUrl : ""

    property bool invalidateArmed: true
    // coverWatchId: GridView can change entryId before we cancel the previous request.
    property string coverWatchId: ""

    function requestCover() {
        if (!root.enabled || !root.visible)
            return
        if (!entryId.length)
            return
        Core.requestCatalogCover(entryId)
    }

    function cancelCover() {
        const id = root.coverWatchId.length ? root.coverWatchId : root.entryId
        if (!id.length)
            return
        Core.cancelCatalogCover(id)
    }

    function onPosterFailed() {
        if (!invalidateArmed || !coverUrl.startsWith("file:"))
            return
        invalidateArmed = false
        Core.invalidateCatalogCover(entryId)
    }

    Timer {
        id: requestTimer
        interval: 60
        onTriggered: root.requestCover()
    }

    // Hover nudge only - no periodic spam while empty.
    Timer {
        id: hoverCoverRetry
        interval: 0
        onTriggered: {
            if (!root.entryId.length || root.displayCoverUrl.length)
                return
            Core.requestCatalogCover(root.entryId)
        }
    }

    function dismissPeek() {
        peekArmTimer.stop()
        peekTimeout.stop()
        scrollSettleTimer.stop()
        root.peekArmed = false
        root.peekWaiting = false
        root.peekScreenshotUrls = []
        screenshotPeek.hideNow()
    }

    function loadPeekScreenshots() {
        if (!root.entryId.length) {
            root.peekScreenshotUrls = []
            return
        }
        const info = Core.catalog.entryInfo(root.entryId)
        root.peekScreenshotUrls = (info && info.screenshotUrls) ? info.screenshotUrls : []
    }

    function refreshPeekAfterEnrich() {
        if (!root.peekArmed && !root.peekWaiting)
            return
        root.loadPeekScreenshots()
        if (root.shotUrls.length > 0) {
            root.peekWaiting = false
            peekTimeout.stop()
        }
    }

    onPeekArmedChanged: {
        if (root.peekArmed)
            root.loadPeekScreenshots()
    }

    onMetadataPendingChanged: {
        if (root.peekArmed || root.peekWaiting)
            root.refreshPeekAfterEnrich()
    }

    // Full enrich does not flip metadataPending when the cover is already local —
    // shelves hit this every time. Reload peek when metadata lands.
    Connections {
        target: Core
        function onEntryMetadataChanged(entryId) {
            if (entryId !== root.entryId)
                return
            root.refreshPeekAfterEnrich()
        }
    }

    // After scroll stops, hover won't re-fire if the pointer never moved.
    function rearmPeekIfHovered() {
        if (root.compactRow || !root.visible || !root.enabled || !posterMouse.containsMouse)
            return
        peekArmTimer.restart()
    }

    // GridView / ListView (catalog + shelves) expose the host flickable here.
    // Discovery shelves also sit in a vertical Flickable - dismiss on that too.
    readonly property var hostFlickable: GridView.view ? GridView.view
                                                       : (ListView.view ? ListView.view : null)

    readonly property var outerFlickable: {
        let p = root.parent
        while (p) {
            if (p !== root.hostFlickable
                    && p.contentY !== undefined
                    && typeof p.moving === "boolean")
                return p
            p = p.parent
        }
        return null
    }

    Timer {
        id: scrollSettleTimer
        interval: 90
        onTriggered: root.rearmPeekIfHovered()
    }

    Connections {
        target: root.hostFlickable
        enabled: root.hostFlickable !== null
        // Avoid contentX/contentY - those fire every pixel for every visible card.
        function onMovementStarted() {
            scrollSettleTimer.stop()
            root.dismissPeek()
        }
        function onFlickStarted() {
            scrollSettleTimer.stop()
            root.dismissPeek()
        }
        function onMovingChanged() {
            if (root.hostFlickable && !root.hostFlickable.moving && !root.hostFlickable.flicking)
                scrollSettleTimer.restart()
        }
        function onFlickingChanged() {
            if (root.hostFlickable && !root.hostFlickable.moving && !root.hostFlickable.flicking)
                scrollSettleTimer.restart()
        }
    }

    Connections {
        target: root.outerFlickable
        enabled: root.outerFlickable !== null
        function onMovementStarted() {
            scrollSettleTimer.stop()
            root.dismissPeek()
        }
        function onFlickStarted() {
            scrollSettleTimer.stop()
            root.dismissPeek()
        }
        function onMovingChanged() {
            if (root.outerFlickable && !root.outerFlickable.moving && !root.outerFlickable.flicking)
                scrollSettleTimer.restart()
        }
        function onFlickingChanged() {
            if (root.outerFlickable && !root.outerFlickable.moving && !root.outerFlickable.flicking)
                scrollSettleTimer.restart()
        }
    }

    Component.onCompleted: {
        root.coverWatchId = root.entryId
        requestTimer.start()
    }
    Component.onDestruction: {
        cancelCover()
        root.dismissPeek()
    }

    function onDelegateRecycled() {
        invalidateArmed = true
        root.dismissPeek()
        requestTimer.restart()
    }

    ListView.onReused: root.onDelegateRecycled()
    ListView.onPooled: {
        requestTimer.stop()
        root.cancelCover()
        root.dismissPeek()
    }
    GridView.onReused: root.onDelegateRecycled()
    GridView.onPooled: {
        requestTimer.stop()
        root.cancelCover()
        root.dismissPeek()
    }

    onVisibleChanged: {
        if (!visible)
            root.dismissPeek()
    }

    onEnabledChanged: {
        if (!enabled)
            root.dismissPeek()
    }

    onEntryIdChanged: {
        if (root.coverWatchId.length && root.coverWatchId !== root.entryId)
            Core.cancelCatalogCover(root.coverWatchId)
        root.coverWatchId = root.entryId
        invalidateArmed = true
        root.dismissPeek()
        requestTimer.restart()
    }

    onDisplayCoverUrlChanged: {
        if (displayCoverUrl.length)
            requestTimer.stop()
    }

    RowLayout {
        anchors.fill: parent
        visible: root.compactRow
        spacing: MD.Token.spacing.medium

        GamePoster {
            Layout.preferredWidth: 52
            Layout.preferredHeight: 70
            source: root.compactRow ? root.displayCoverUrl : ""
            seed: root.title
            fallbackText: root.title.length > 0 ? root.title.charAt(0) : "?"
            awaiting: root.metadataPending
            enableShimmer: false
            hoverScaleEnabled: false
            cornerRadius: MD.Token.shape.corner.medium
            decodeWidth: 104
            decodeHeight: 140
            onClicked: root.openDetails(root.entryId)
            onLoadFailed: root.onPosterFailed()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            MD.Label {
                Layout.fillWidth: true
                text: root.title
                typescale: MD.Token.typescale.title_small
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            MD.Label {
                Layout.fillWidth: true
                text: root.metaLine
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_medium
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: MD.Token.spacing.small
        visible: !root.compactRow
        spacing: MD.Token.spacing.extra_small

        Item {
            id: posterHost
            Layout.fillWidth: true
            Layout.fillHeight: true

            GamePoster {
                id: gridPoster
                anchors.fill: parent
                source: root.compactRow ? "" : root.displayCoverUrl
                seed: root.title
                fallbackText: root.title.length > 0 ? root.title.charAt(0) : "?"
                awaiting: root.metadataPending
                enableShimmer: true
                cornerRadius: MD.Token.shape.corner.large
                hoverScaleEnabled: false
                decodeWidth: Math.max(120, Math.round(width * 1.25))
                decodeHeight: Math.max(160, Math.round(height * 1.25))
                // Card owns a full-size MouseArea above MD.Image (layer/RoundClip
                // + hover scrim made poster's own hover only work in a tiny zone).
                inputEnabled: false
                externalHovered: posterMouse.containsMouse
                onLoadFailed: root.onPosterFailed()
            }

            MouseArea {
                id: posterMouse
                anchors.fill: parent
                z: 20
                enabled: !root.compactRow && root.visible && root.enabled
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.openDetails(root.entryId)
                onContainsMouseChanged: {
                    if (!enabled) {
                        root.dismissPeek()
                        return
                    }
                    if (containsMouse) {
                        if (!root.displayCoverUrl.length)
                            hoverCoverRetry.restart()
                        peekArmTimer.restart()
                    } else {
                        root.dismissPeek()
                    }
                }
            }

            Timer {
                id: peekArmTimer
                interval: 220
                repeat: false
                onTriggered: {
                    if (!posterMouse.containsMouse || !root.visible || !root.enabled)
                        return
                    root.loadPeekScreenshots()
                    root.peekArmed = true
                    if (root.shotUrls.length > 0) {
                        root.peekWaiting = false
                        return
                    }
                    root.peekWaiting = true
                    Core.enrichCatalogEntry(root.entryId)
                    // Cache hits emit metadataReady synchronously inside enrich —
                    // entryInfo is empty until that apply finishes; pull next tick.
                    Qt.callLater(root.refreshPeekAfterEnrich)
                    peekTimeout.restart()
                }
            }

            Timer {
                id: peekTimeout
                interval: 12000
                repeat: false
                onTriggered: root.peekWaiting = false
            }

            // Soft wait cue on the poster only - never a blocking gray popup.
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
                visible: root.peekWaiting && posterMouse.containsMouse && root.shotUrls.length === 0
                running: visible
                color: MD.Token.color.primary
                trackColor: MD.Util.transparent(MD.Token.color.on_surface, 0.18)
            }

            CatalogScreenshotPeek {
                id: screenshotPeek
                entryId: root.entryId
                anchorItem: posterHost
                urls: root.shotUrls
                leftEdgeX: root.peekLeftEdge
                // Only after we have URLs - popup itself waits for Image.Ready
                // and rejects frames that don't belong to this entryId.
                active: root.peekArmed && posterMouse.containsMouse && root.shotUrls.length > 0
            }
        }

        MD.Label {
            Layout.fillWidth: true
            text: root.title
            typescale: MD.Token.typescale.title_small
            elide: Text.ElideRight
            maximumLineCount: 1
            opacity: posterMouse.containsMouse ? 1 : 0.92

            Behavior on opacity {
                NumberAnimation {
                    duration: MD.Token.duration.short3
                    easing: MD.Token.easing.emphasized_decelerate
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            text: root.metaLine
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_medium
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }

    MouseArea {
        anchors.fill: parent
        visible: root.compactRow
        cursorShape: Qt.PointingHandCursor
        onClicked: root.openDetails(root.entryId)
        z: -1
    }
}
