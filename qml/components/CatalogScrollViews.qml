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
    property alias gridContentHeight: grid.contentHeight
    property alias listContentHeight: list.contentHeight
    property alias gridHeight: grid.height
    property alias listHeight: list.height

    readonly property int scrubberWidth: catalogNavigator.visible ? catalogNavigator.width : 0
    readonly property real compactBarHeight: 56
    readonly property bool catalogBound: page.catalogModelReady && page.enabled
                                         && !page.discoveryMode && !Core.catalog.bulkUpdating

    // Discrete chrome mode with hysteresis - avoids per-pixel bounce while flicking.
    property bool scrubberExpanded: true
    property real expandedHeaderHeight: 140
    property int navigatorRow: 0
    property real navigatorViewportFraction: 0.02

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
        // Match early compact-bar reveal, with hysteresis to avoid bounce.
        if (root.scrubberExpanded && y > 10)
            root.scrubberExpanded = false
        else if (!root.scrubberExpanded && y < 10)
            root.scrubberExpanded = true
        root.syncNavigatorPosition()
    }

    function firstVisibleRow(view) {
        if (!view)
            return 0
        const headerH = view.headerItem ? view.headerItem.height : 0
        const y = view.contentY + headerH + 8
        const idx = view.indexAt(Math.min(24, view.width * 0.5), y)
        if (idx >= 0)
            return idx
        // Fallback when indexAt misses between cells.
        const count = Core.catalog.count
        if (count <= 0)
            return 0
        const body = Math.max(1, view.contentHeight - headerH)
        const t = Math.max(0, Math.min(1, (view.contentY) / Math.max(1, body - view.height)))
        return Math.round(t * (count - 1))
    }

    function syncNavigatorPosition() {
        if (catalogNavigator.scrubbing)
            return
        const view = activeView()
        const count = Core.catalog.count
        if (!view || count <= 0) {
            root.navigatorRow = 0
            root.navigatorViewportFraction = 1
            return
        }
        root.navigatorRow = Math.max(0, Math.min(count - 1, root.firstVisibleRow(view)))
        const headerH = view.headerItem ? view.headerItem.height : 0
        const body = Math.max(1, view.contentHeight - headerH)
        root.navigatorViewportFraction = Math.max(0.02, Math.min(1, view.height / body))
    }

    function jumpToRow(row, animated) {
        if (row === undefined || row < 0 || row >= Core.catalog.count)
            return false

        const view = activeView()
        if (!view || view.height < 8)
            return false
        const mode = page.listViewMode ? ListView.Beginning : GridView.Beginning
        root.navigatorRow = row

        const fromY = view.contentY
        const maxY = Math.max(0, view.contentHeight - view.height)
        // Grid not laid out yet - caller should retry.
        if (row > 0 && maxY < 8 && Core.catalog.count > 32)
            return false

        let toY = 0
        // Row 0 must reveal the catalog header (true top), not just the first cell.
        if (row > 0) {
            view.positionViewAtIndex(row, mode)
            const newMax = Math.max(0, view.contentHeight - view.height)
            toY = Math.max(0, Math.min(view.contentY, newMax))
            view.contentY = fromY
        }

        // Scrub: exponential ease toward target (smooth, interruptible).
        if (catalogNavigator.scrubbing) {
            indexScrollAnim.stop()
            scrubGoalY = toY
            scrubLerp.running = true
            return true
        }

        scrubLerp.running = false
        if (animated === false) {
            view.contentY = toY
            root.updateScrubberMode()
            return true
        }

        indexScrollAnim.stop()
        indexScrollAnim.target = view
        indexScrollAnim.from = fromY
        indexScrollAnim.to = toY
        indexScrollAnim.duration = Math.min(420, Math.max(160, Math.abs(toY - fromY) * 0.28))
        indexScrollAnim.start()
        return true
    }

    function restoreContentY(y) {
        const view = activeView()
        if (!view || view.height < 8)
            return false
        const maxY = Math.max(0, view.contentHeight - view.height)
        // Layout still collapsed after StackView - wait, don't clamp to a fake top.
        if (y > 8 && maxY + 1 < y)
            return false
        view.contentY = Math.min(Math.max(0, y), maxY)
        root.updateScrubberMode()
        return Math.abs(view.contentY - Math.min(y, maxY)) < 2
    }

    function jumpToEdge(edge) {
        if (edge === 0)
            root.jumpToRow(0, true)
        else if (Core.catalog.count > 0)
            root.jumpToRow(Core.catalog.count - 1, true)
    }

    function stopProgrammaticScroll() {
        indexScrollAnim.stop()
        scrubLerp.running = false
    }

    property real scrubGoalY: 0

    Timer {
        id: scrubLerp
        interval: 16
        repeat: true
        running: false
        onTriggered: {
            // Never fight wheel / touchpad / drag scrolling.
            if (!catalogNavigator.scrubbing) {
                running = false
                return
            }
            const view = root.activeView()
            if (!view) {
                running = false
                return
            }
            const cur = view.contentY
            const goal = root.scrubGoalY
            const d = goal - cur
            if (Math.abs(d) < 0.75) {
                view.contentY = goal
                running = false
                root.updateScrubberMode()
                return
            }
            view.contentY = cur + d * (catalogNavigator.precision ? 0.22 : 0.32)
        }
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

    Timer {
        id: navigatorSyncTimer
        interval: 32
        onTriggered: root.syncNavigatorPosition()
    }

    GridView {
        id: grid
        anchors.fill: parent
        anchors.leftMargin: pageMargin
        anchors.rightMargin: pageMargin + root.scrubberWidth
        anchors.bottomMargin: MD.Token.spacing.medium
        visible: !page.listViewMode
        clip: true
        // Unbind off-tab / discovery only. Do NOT key off root.visible - StackView
        // covering the page must not drop the 132k model (that zeros contentY and
        // cancels every visible cover).
        model: (root.catalogBound && !page.listViewMode) ? Core.catalog : null
        cellWidth: page.cellWidth
        cellHeight: page.cellHeight
        cacheBuffer: page.cellHeight * 2
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        pixelAligned: true
        interactive: true

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
                sortOptions: page.sortOptions
                onFilterRequested: page.openFilterSheet()
                onViewModeChangeRequested: function (mode) { prefs.viewMode = mode }
                onRefreshRequested: Core.refreshSelectedCatalogs()
                onSortModeRequested: function (mode) { page.applySortMode(mode) }
            }

            Component.onCompleted: headerMeasureTimer.start()
            onHeightChanged: headerMeasureTimer.restart()
        }

        // Small catalogs keep a normal bar; huge lists use CatalogNavigator.
        ScrollBar.vertical: MD.ScrollBar {
            policy: catalogNavigator.visible ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
            interactive: true
        }

        onContentYChanged: {
            root.updateScrubberMode()
            navigatorSyncTimer.restart()
        }
        onMovementStarted: root.stopProgrammaticScroll()
        onFlickStarted: root.stopProgrammaticScroll()
        onMovingChanged: {
            if (!moving)
                root.updateScrubberMode()
        }

        delegate: CatalogGameCard {
            width: page.cardWidth
            height: page.cardHeight
            peekLeftEdge: page.peekLeftEdge ?? 0
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
        model: (root.catalogBound && page.listViewMode) ? Core.catalog : null
        spacing: MD.Token.spacing.extra_small
        cacheBuffer: page.listRowHeight * 3
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: true

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
                sortOptions: page.sortOptions
                onFilterRequested: page.openFilterSheet()
                onViewModeChangeRequested: function (mode) { prefs.viewMode = mode }
                onRefreshRequested: Core.refreshSelectedCatalogs()
                onSortModeRequested: function (mode) { page.applySortMode(mode) }
            }

            Component.onCompleted: headerMeasureTimer.start()
            onHeightChanged: headerMeasureTimer.restart()
        }

        ScrollBar.vertical: MD.ScrollBar {
            policy: catalogNavigator.visible ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
            interactive: true
        }

        onContentYChanged: {
            root.updateScrubberMode()
            navigatorSyncTimer.restart()
        }
        onMovementStarted: root.stopProgrammaticScroll()
        onFlickStarted: root.stopProgrammaticScroll()
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

    CatalogNavigator {
        id: catalogNavigator
        anchors.right: parent.right
        anchors.rightMargin: Math.max(2, pageMargin * 0.2)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.scrubberTopMargin
        anchors.bottomMargin: MD.Token.spacing.medium
        catalogCount: Core.catalog.count
        stops: Core.catalog.count >= 32 ? Core.catalog.scrubStops : []
        currentRow: root.navigatorRow
        viewportFraction: root.navigatorViewportFraction
        z: 3
        onJumpToRow: function (row) { root.jumpToRow(row) }
        onJumpToEdge: function (edge) { root.jumpToEdge(edge) }
        onScrubbingChanged: {
            if (scrubbing)
                return
            // Finish the ease and release control back to wheel / flick.
            if (scrubLerp.running) {
                const view = root.activeView()
                if (view)
                    view.contentY = root.scrubGoalY
                scrubLerp.running = false
                root.updateScrubberMode()
            }
        }
    }

    Component.onCompleted: Qt.callLater(root.syncNavigatorPosition)
}
