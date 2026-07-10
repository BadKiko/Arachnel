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

    // Debounce so a fast flick does not enqueue every recycled cell.
    Timer {
        id: requestTimer
        interval: 60
        onTriggered: root.requestCoverIfNeeded()
    }

    Component.onCompleted: requestTimer.start()
    Component.onDestruction: cancelRequest()

    // reuseItems: cancel when pooled; re-request when shown again with new model data.
    GridView.onPooled: cancelRequest()
    GridView.onReused: requestTimer.restart()

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

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: MD.Token.spacing.small
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
}
