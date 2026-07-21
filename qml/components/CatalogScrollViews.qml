import QtQuick
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property var page
    required property var prefs
    required property int pageMargin

    property alias gridContentY: grid.contentY
    property alias listContentY: list.contentY

    GridView {
        id: grid
        anchors.fill: parent
        anchors.leftMargin: pageMargin
        anchors.rightMargin: pageMargin
        anchors.bottomMargin: MD.Token.spacing.medium
        visible: !page.listViewMode
        clip: true
        model: Core.catalog
        cellWidth: page.cellWidth
        cellHeight: page.cellHeight
        cacheBuffer: page.cellHeight * 2
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        pixelAligned: true

        header: Column {
            width: grid.width
            spacing: MD.Token.spacing.small

            CatalogIntroHeader {
                width: parent.width
            }

            CatalogScrollHeader {
                contentWidth: grid.width
                hasSelection: Core.activeCatalogSourceIds.length > 0
                listViewMode: page.listViewMode
                onFilterRequested: page.openFilterSheet()
                onViewModeChangeRequested: function (mode) { prefs.viewMode = mode }
                onRefreshRequested: Core.refreshSelectedCatalogs()
            }
        }

        ScrollBar.vertical: MD.ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: CatalogGameCard {
            width: page.cardWidth
            height: page.cardHeight
            onOpenDetails: function (id) { page.openGame(id) }
        }
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.leftMargin: pageMargin
        anchors.rightMargin: pageMargin
        anchors.bottomMargin: MD.Token.spacing.medium
        visible: page.listViewMode
        clip: true
        model: Core.catalog
        spacing: MD.Token.spacing.extra_small
        cacheBuffer: page.listRowHeight * 8
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds

        header: Column {
            width: list.width
            spacing: MD.Token.spacing.small

            CatalogIntroHeader {
                width: parent.width
            }

            CatalogScrollHeader {
                contentWidth: list.width
                hasSelection: Core.activeCatalogSourceIds.length > 0
                listViewMode: page.listViewMode
                onFilterRequested: page.openFilterSheet()
                onViewModeChangeRequested: function (mode) { prefs.viewMode = mode }
                onRefreshRequested: Core.refreshSelectedCatalogs()
            }
        }

        ScrollBar.vertical: MD.ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: CatalogGameCard {
            width: list.width
            height: page.listRowHeight
            compactRow: true
            onOpenDetails: function (id) { page.openGame(id) }
        }
    }
}
