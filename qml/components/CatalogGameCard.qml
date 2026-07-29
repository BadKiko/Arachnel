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
    // Must be required - GridView only injects model roles into required props.
    required property var screenshotUrls

    property bool compactRow: false
    property int currentPlayers: -1
    property bool peekArmed: false
    property bool peekWaiting: false

    signal openDetails(string entryId)

    readonly property var shotUrls: {
        const raw = root.screenshotUrls
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

    function requestCover() {
        if (!entryId.length)
            return
        if (displayCoverUrl.length)
            return
        Core.requestCatalogCover(entryId)
    }

    function cancelCover() {
        if (!entryId.length)
            return
        Core.cancelCatalogCover(entryId)
    }

    function onPosterFailed() {
        if (!invalidateArmed || !coverUrl.startsWith("file:"))
            return
        invalidateArmed = false
        Core.invalidateCatalogCover(entryId)
    }

    Timer {
        id: requestTimer
        interval: 40
        onTriggered: root.requestCover()
    }

    function dismissPeek() {
        peekArmTimer.stop()
        peekTimeout.stop()
        scrollSettleTimer.stop()
        root.peekArmed = false
        root.peekWaiting = false
        screenshotPeek.hideNow()
    }

    // After scroll stops, HoverHandler won't re-fire if the pointer never moved.
    function rearmPeekIfHovered() {
        if (root.compactRow || !root.visible || !posterHover.hovered)
            return
        peekArmTimer.restart()
    }

    // GridView / ListView (catalog + shelves) expose the host flickable here.
    readonly property var hostFlickable: GridView.view ? GridView.view
                                                       : (ListView.view ? ListView.view : null)

    Timer {
        id: scrollSettleTimer
        interval: 90
        onTriggered: root.rearmPeekIfHovered()
    }

    Connections {
        target: root.hostFlickable
        enabled: root.hostFlickable !== null
        function onMovementStarted() {
            scrollSettleTimer.stop()
            root.dismissPeek()
        }
        function onFlickStarted() {
            scrollSettleTimer.stop()
            root.dismissPeek()
        }
        function onContentXChanged() {
            root.dismissPeek()
            scrollSettleTimer.restart()
        }
        function onContentYChanged() {
            root.dismissPeek()
            scrollSettleTimer.restart()
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

    Component.onCompleted: requestTimer.start()
    Component.onDestruction: {
        cancelCover()
        root.dismissPeek()
    }

    // GridView reuse was missing — pooled cards left Overlay popups behind.
    function onDelegateRecycled() {
        invalidateArmed = true
        root.dismissPeek()
        requestTimer.restart()
    }

    ListView.onReused: root.onDelegateRecycled()
    ListView.onPooled: root.dismissPeek()
    GridView.onReused: root.onDelegateRecycled()
    GridView.onPooled: root.dismissPeek()

    onVisibleChanged: {
        if (!visible)
            root.dismissPeek()
    }

    onEntryIdChanged: {
        cancelCover()
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
                onClicked: root.openDetails(root.entryId)
                onLoadFailed: root.onPosterFailed()
            }

            HoverHandler {
                id: posterHover
                enabled: !root.compactRow && root.visible
                onHoveredChanged: {
                    if (hovered) {
                        peekArmTimer.restart()
                    } else {
                        root.dismissPeek()
                    }
                }
            }

            Timer {
                id: peekArmTimer
                interval: 380
                repeat: false
                onTriggered: {
                    if (!posterHover.hovered || !root.visible)
                        return
                    root.peekArmed = true
                    if (root.shotUrls.length > 0) {
                        root.peekWaiting = false
                        return
                    }
                    root.peekWaiting = true
                    Core.enrichCatalogEntry(root.entryId)
                    peekTimeout.restart()
                }
            }

            Timer {
                id: peekTimeout
                interval: 6500
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
                visible: root.peekWaiting && posterHover.hovered && root.shotUrls.length === 0
                running: visible
                color: MD.Token.color.primary
                trackColor: MD.Util.transparent(MD.Token.color.on_surface, 0.18)
            }

            CatalogScreenshotPeek {
                id: screenshotPeek
                entryId: root.entryId
                anchorItem: posterHost
                urls: root.shotUrls
                // Only after we have URLs - popup itself waits for Image.Ready
                // and rejects frames that don't belong to this entryId.
                active: root.peekArmed && posterHover.hovered && root.shotUrls.length > 0
            }
        }

        MD.Label {
            Layout.fillWidth: true
            text: root.title
            typescale: MD.Token.typescale.title_small
            elide: Text.ElideRight
            maximumLineCount: 1
            opacity: posterHover.hovered ? 1 : 0.92

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
