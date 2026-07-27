import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property string title
    required property string subtitle
    required property string iconName
    required property var shelfModel
    required property var page

    spacing: MD.Token.spacing.small
    visible: shelfModel && shelfModel.count > 0

    RowLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.medium

        MD.Icon {
            name: root.iconName
            size: 22
            color: MD.Token.color.primary
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            MD.Label {
                Layout.fillWidth: true
                text: root.title
                typescale: MD.Token.typescale.title_medium
            }

            MD.Label {
                Layout.fillWidth: true
                visible: root.subtitle.length > 0
                text: root.subtitle
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_small
            }
        }

        MD.Label {
            text: shelfModel.count
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }
    }

    ListView {
        id: shelfList
        Layout.fillWidth: true
        Layout.preferredHeight: page.cardHeight
        orientation: ListView.Horizontal
        clip: true
        spacing: MD.Token.spacing.medium
        model: root.shelfModel
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        cacheBuffer: page.cardWidth * 2

        ScrollBar.horizontal: MD.ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: CatalogGameCard {
            width: page.cardWidth
            height: page.cardHeight
            onOpenDetails: function (id) { page.openGame(id) }
        }
    }
}
