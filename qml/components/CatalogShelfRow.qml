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

    signal surpriseFromShelf(string entryId)

    spacing: MD.Token.spacing.small
    visible: shelfModel && shelfModel.count > 0

    readonly property real scrollStep: page.cardWidth + shelfList.spacing
    readonly property bool canScrollLeft: shelfList.contentX > 1
    readonly property bool canScrollRight: shelfList.contentWidth - shelfList.width - shelfList.contentX > 1

    function scrollBy(delta) {
        const maxX = Math.max(0, shelfList.contentWidth - shelfList.width)
        shelfList.contentX = Math.max(0, Math.min(maxX, shelfList.contentX + delta))
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.small

        MD.Label {
            Layout.fillWidth: true
            text: root.title
            typescale: MD.Token.typescale.title_medium
            elide: Text.ElideRight
        }

        MD.IconButton {
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.arrow_back
            enabled: root.canScrollLeft
            visible: shelfList.contentWidth > shelfList.width
            onClicked: root.scrollBy(-root.scrollStep)
        }

        MD.IconButton {
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.chevron_right
            enabled: root.canScrollRight
            visible: shelfList.contentWidth > shelfList.width
            onClicked: root.scrollBy(root.scrollStep)
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
        // Carousel arrows replace the scrollbar.
        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AlwaysOff
        }

        Behavior on contentX {
            NumberAnimation {
                duration: MD.Token.duration.medium2
                easing: MD.Token.easing.emphasized_decelerate
            }
        }

        delegate: CatalogGameCard {
            width: page.cardWidth
            height: page.cardHeight
            onOpenDetails: function (id) { page.openGame(id) }
        }
    }
}
