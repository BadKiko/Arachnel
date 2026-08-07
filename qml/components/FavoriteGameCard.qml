import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property string gameId
    required property string title
    required property string coverUrl
    required property string sourceName

    signal openDetails(string gameId)

    readonly property string displayTitle: {
        const t = (root.title || "").trim()
        if (t.length && t !== root.gameId)
            return t
        const info = Core.entryDetails(root.gameId)
        const live = String(info.title || "").trim()
        return live.length ? live : (t.length ? t : root.gameId)
    }

    readonly property string displayCoverUrl: {
        const local = (root.coverUrl || "")
        if (local.startsWith("file:"))
            return local
        const info = Core.entryDetails(root.gameId)
        const live = String(info.coverUrl || "")
        if (live.startsWith("file:"))
            return live
        return local.startsWith("file:") ? local : ""
    }

    readonly property string displaySourceName: {
        const s = (root.sourceName || "").trim()
        if (s.length)
            return s
        const info = Core.entryDetails(root.gameId)
        return String(info.sourceName || info.sourceId || "")
    }

    function requestCover() {
        if (!root.gameId.length || !root.visible || !root.enabled)
            return
        Core.requestCatalogCover(root.gameId)
    }

    Timer {
        id: coverTimer
        interval: 60
        onTriggered: root.requestCover()
    }

    Component.onCompleted: coverTimer.start()
    onGameIdChanged: coverTimer.restart()
    onVisibleChanged: {
        if (visible)
            coverTimer.restart()
    }

    Connections {
        target: Core
        function onEntryMetadataChanged(entryId) {
            if (entryId === root.gameId)
                coverTimer.restart()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: MD.Token.spacing.small
        spacing: MD.Token.spacing.small

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GamePoster {
                anchors.fill: parent
                source: root.displayCoverUrl
                seed: root.displayTitle
                fallbackText: root.displayTitle.length ? root.displayTitle.charAt(0) : "?"
                cornerRadius: MD.Token.shape.corner.large
                hoverScaleEnabled: true
                onClicked: root.openDetails(root.gameId)
                onLoadFailed: {
                    if (root.displayCoverUrl.startsWith("file:"))
                        Core.invalidateCatalogCover(root.gameId)
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            text: root.displayTitle
            typescale: MD.Token.typescale.title_small
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        MD.Label {
            Layout.fillWidth: true
            visible: root.displaySourceName.length > 0
            text: root.displaySourceName
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_medium
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }
}
