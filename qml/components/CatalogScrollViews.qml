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

    readonly property int scrubberWidth: indexScrubber.visible ? indexScrubber.width : 0
    readonly property real compactBarHeight: 56

    // Discrete chrome mode with hysteresis - avoids per-pixel bounce while flicking.
    property bool scrubberExpanded: true
    property real expandedHeaderHeight: 140

    readonly property real scrubberTopClearance: {
        if (root.scrubberExpanded && page.compactBarOpacity < 0.25)
            return root.expandedHeaderHeight
        return root.compactBarHeight * page.compactBarOpacity
    }

    property real scrubberTopMargin: MD.Token.spacing.small + scrubberTopClearance

    Behavior on scrubberTopMargin {
        NumberAnimation {
            duration: MD.Token.duration.medium2
            easing.type: Easing.OutCubic
        }
    }

    function activeView() {
        return page.listViewMode ? list : grid
    }

    function syncHeaderHeight() {
        const header = activeView().headerItem
        if (header && header.height > 40)
            root.expandedHeaderHeight = header.height
    }

    function updateScrubberMode() {
        const y = activeView().contentY
        // Match early compact-bar reveal (~8px), with hysteresis to avoid bounce.
        if (root.scrubberExpanded && y > 10)
            root.scrubberExpanded = false
        else if (!root.scrubberExpanded && y < 2)
            root.scrubberExpanded = true
    }

    function jumpToRow(row) {
        if (row === undefined || row < 0 || row >= Core.catalog.count)
            return

        const view = activeView()
        const mode = page.listViewMode ? ListView.Beginning : GridView.Beginning
        const fromY = view.contentY
        view.positionViewAtIndex(row, mode)
        const maxY = Math.max(0, view.contentHeight - view.height)
        const toY = Math.max(0, Math.min(view.contentY, maxY))
        view.contentY = fromY

        indexScrollAnim.stop()
        indexScrollAnim.target = view
        indexScrollAnim.from = fromY
        indexScrollAnim.to = toY
        indexScrollAnim.duration = Math.min(420, Math.max(180, Math.abs(toY - fromY) * 0.35))
        indexScrollAnim.start()
    }

    NumberAnimation {
        id: indexScrollAnim
        property: "contentY"
        easing.type: Easing.OutCubic
        onStopped: root.updateScrubberMode()
    }

    Timer {
        id: headerMeasureTimer
        interval: 0
        onTriggered: root.syncHeaderHeight()
    }

    GridView {
        id: grid
        anchors.fill: parent
        anchors.leftMargin: pageMargin
        anchors.rightMargin: pageMargin + root.scrubberWidth
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
                collapseProgress: page.introCollapseProgress
            }

            CatalogScrollHeader {
                contentWidth: grid.width
                hasSelection: Core.activeCatalogSourceIds.length > 0
                listViewMode: page.listViewMode
                collapseProgress: page.compactBarOpacity
                onFilterRequested: page.openFilterSheet()
                onViewModeChangeRequested: function (mode) { prefs.viewMode = mode }
                onRefreshRequested: Core.refreshSelectedCatalogs()
            }

            Component.onCompleted: headerMeasureTimer.start()
            onHeightChanged: headerMeasureTimer.restart()
        }

        ScrollBar.vertical: MD.ScrollBar {
            policy: ScrollBar.AlwaysOff
        }

        onContentYChanged: root.updateScrubberMode()
        onMovementStarted: indexScrollAnim.stop()
        onMovingChanged: {
            if (!moving)
                root.updateScrubberMode()
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
        anchors.rightMargin: pageMargin + root.scrubberWidth
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
                collapseProgress: page.introCollapseProgress
            }

            CatalogScrollHeader {
                contentWidth: list.width
                hasSelection: Core.activeCatalogSourceIds.length > 0
                listViewMode: page.listViewMode
                collapseProgress: page.compactBarOpacity
                onFilterRequested: page.openFilterSheet()
                onViewModeChangeRequested: function (mode) { prefs.viewMode = mode }
                onRefreshRequested: Core.refreshSelectedCatalogs()
            }

            Component.onCompleted: headerMeasureTimer.start()
            onHeightChanged: headerMeasureTimer.restart()
        }

        ScrollBar.vertical: MD.ScrollBar {
            policy: ScrollBar.AlwaysOff
        }

        onContentYChanged: root.updateScrubberMode()
        onMovementStarted: indexScrollAnim.stop()
        onMovingChanged: {
            if (!moving)
                root.updateScrubberMode()
        }

        delegate: CatalogGameCard {
            width: list.width
            height: page.listRowHeight
            compactRow: true
            onOpenDetails: function (id) { page.openGame(id) }
        }
    }

    CatalogIndexScrubber {
        id: indexScrubber
        anchors.right: parent.right
        anchors.rightMargin: Math.max(2, pageMargin * 0.25)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.scrubberTopMargin
        anchors.bottomMargin: MD.Token.spacing.medium
        stops: Core.catalog.count >= 32 ? Core.catalog.scrubStops : []
        z: 3
        onStopSelected: function (row, label) {
            root.jumpToRow(row)
        }
    }
}
