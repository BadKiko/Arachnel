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
    required property string sizeLabel
    required property string installKindLabel
    required property bool metadataPending

    property bool compactRow: false

    signal openDetails(string entryId)

    readonly property bool hasLibraryCover: coverUrl.startsWith("file:")
                                            || coverUrl.indexOf("library_capsule") >= 0
                                            || coverUrl.indexOf("library_600x900") >= 0

    property string requestedId: ""

    function requestCoverIfNeeded() {
        if (!entryId.length || hasLibraryCover)
            return
        requestedId = entryId
        Core.requestCatalogCover(entryId)
    }

    function cancelRequest() {
        requestTimer.stop()
        if (!requestedId.length)
            return
        Core.cancelCatalogCover(requestedId)
        requestedId = ""
    }

    Timer {
        id: requestTimer
        interval: 60
        onTriggered: root.requestCoverIfNeeded()
    }

    Component.onCompleted: requestTimer.start()
    Component.onDestruction: cancelRequest()

    ListView.onReused: requestTimer.restart()

    onEntryIdChanged: {
        cancelRequest()
        requestTimer.restart()
    }

    onCoverUrlChanged: {
        if (hasLibraryCover) {
            requestTimer.stop()
            requestedId = ""
        }
    }

    RowLayout {
        anchors.fill: parent
        visible: root.compactRow
        spacing: MD.Token.spacing.medium

        GamePoster {
            Layout.preferredWidth: 52
            Layout.preferredHeight: 70
            source: root.coverUrl
            seed: root.title
            fallbackText: root.title.length > 0 ? root.title.charAt(0) : "?"
            awaiting: root.metadataPending
            cornerRadius: MD.Token.shape.corner.medium
            onClicked: root.openDetails(root.entryId)
            onLoadFailed: Core.invalidateCatalogCover(root.entryId)
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
                text: "v" + root.version + " · " + root.sizeLabel
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_medium
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        MD.Label {
            text: root.installKindLabel
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_small
            elide: Text.ElideRight
            maximumLineCount: 1
            Layout.maximumWidth: 120
        }

        MD.Icon {
            name: MD.Token.icon.chevron_right
            size: 20
            color: MD.Token.color.on_surface_variant
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
            source: root.coverUrl
            seed: root.title
            fallbackText: root.title.length > 0 ? root.title.charAt(0) : "?"
            awaiting: root.metadataPending
            cornerRadius: MD.Token.shape.corner.large
            onClicked: root.openDetails(root.entryId)
            onLoadFailed: Core.invalidateCatalogCover(root.entryId)
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
            text: "v" + root.version + " · " + root.sizeLabel
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
