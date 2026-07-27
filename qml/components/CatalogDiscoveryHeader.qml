import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var page
    spacing: MD.Token.spacing.medium

    signal browseAllRequested()

    readonly property bool showSkeleton: Core.catalogDiscovery.loading
    readonly property bool noFeed: !Core.catalogDiscovery.feedLoaded
                                   && !Core.catalogDiscovery.loading

    RowLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.medium

        ColumnLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.extra_small

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Discover")
                typescale: MD.Token.typescale.headline_medium
            }

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("A few shelves to browse. Open the full catalog anytime.")
                wrapMode: Text.WordWrap
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
            }
        }

        MD.Button {
            mdState.type: MD.Enum.BtFilled
            text: qsTr("All games")
            onClicked: root.browseAllRequested()
        }
    }

    CatalogSourceChips {
        Layout.fillWidth: true
    }

    CatalogDiscoveryHero {
        Layout.fillWidth: true
        visible: !root.showSkeleton && !root.noFeed
                 && Core.catalogDiscovery.onFireShelf.count > 0
        page: root.page
        shelfModel: Core.catalogDiscovery.onFireShelf
        onOpenGame: function (id) { page.openGame(id) }
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
            text: qsTr("Couldn't load discovery")
            typescale: MD.Token.typescale.title_medium
        }

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Check your connection, or open All games.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
    }
}
