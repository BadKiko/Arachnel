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

    signal openDetails(string entryId)

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

    Component.onCompleted: requestTimer.start()
    Component.onDestruction: cancelCover()

    ListView.onReused: {
        invalidateArmed = true
        requestTimer.restart()
    }

    onEntryIdChanged: {
        cancelCover()
        invalidateArmed = true
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

        GamePoster {
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: root.compactRow ? "" : root.displayCoverUrl
            seed: root.title
            fallbackText: root.title.length > 0 ? root.title.charAt(0) : "?"
            awaiting: root.metadataPending
            enableShimmer: true
            cornerRadius: MD.Token.shape.corner.large
            onClicked: root.openDetails(root.entryId)
            onLoadFailed: root.onPosterFailed()
        }

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

    MouseArea {
        anchors.fill: parent
        visible: root.compactRow
        cursorShape: Qt.PointingHandCursor
        onClicked: root.openDetails(root.entryId)
        z: -1
    }
}
