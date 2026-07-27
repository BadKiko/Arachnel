import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var page
    spacing: MD.Token.spacing.large

    signal browseAllRequested()
    signal whatToPlayRequested()

    readonly property bool showSkeleton: Core.catalogDiscovery.loading
    readonly property bool noFeed: !Core.catalogDiscovery.feedLoaded
                                   && !Core.catalogDiscovery.loading

    CatalogDiscoveryHero {
        Layout.fillWidth: true
        visible: !root.showSkeleton && !root.noFeed
                 && Core.catalogDiscovery.onFireShelf.count > 0
        page: root.page
        shelfModel: Core.catalogDiscovery.onFireShelf
        onOpenGame: function (id) { page.openGame(id) }
    }

    CatalogMoodChips {
        Layout.fillWidth: true
        visible: !root.noFeed
        page: root.page
        onWhatToPlayRequested: root.whatToPlayRequested()
    }

    CatalogDiscoverySkeleton {
        Layout.fillWidth: true
        visible: root.showSkeleton
        page: root.page
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.noFeed
        spacing: MD.Token.spacing.small

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Discovery feed not installed yet")
            typescale: MD.Token.typescale.title_medium
        }

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Shelves will come from a backend feed (discovery-feed.json). Use Browse all games for now.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        MD.Button {
            mdState.type: MD.Enum.BtOutlined
            text: qsTr("Browse all games")
            onClicked: root.browseAllRequested()
        }
    }
}
