import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtCore

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int cellWidth: 176
    readonly property int cellHeight: 284
    readonly property int cardWidth: 160
    readonly property int cardHeight: 268
    readonly property int listRowHeight: 80
    readonly property int pageMargin: MD.Token.spacing.large
    readonly property int cardRadius: MD.Token.shape.corner.extra_large
    readonly property bool noSources: Core.sources.enabledCount === 0
    readonly property bool noSourceSelected: Core.activeCatalogSourceIds.length === 0
    readonly property bool catalogEmpty: !Core.catalogLoading
                                         && (root.noSourceSelected
                                             || (!root.noSourceSelected && Core.catalog.count === 0))
    readonly property bool listViewMode: catalogPrefs.viewMode === 1

    readonly property int compactRevealRange: 28
    readonly property real scrollContentY: root.listViewMode ? list.contentY : grid.contentY
    readonly property real compactRevealStart: root.catalogIntroHeaderHeight + MD.Token.spacing.small
    readonly property real compactBarOpacity: {
        const sc = root.listViewMode ? list : grid
        if (!sc.visible)
            return 0
        const y = sc.contentY
        if (y <= root.compactRevealStart)
            return 0
        return Math.min(1, (y - root.compactRevealStart) / root.compactRevealRange)
    }

    readonly property var sortOptions: [
        { mode: 0, label: qsTr("Newest first") },
        { mode: 1, label: qsTr("Oldest first") },
        { mode: 2, label: qsTr("Title A–Z") },
        { mode: 3, label: qsTr("Title Z–A") },
        { mode: 4, label: qsTr("Portable first") },
        { mode: 5, label: qsTr("Non-portable first") },
        { mode: 6, label: qsTr("Largest first") },
        { mode: 7, label: qsTr("Smallest first") }
    ]

    readonly property var typeFilterLabels: ({
        "-1": qsTr("All"),
        "0": qsTr("Portable"),
        "1": qsTr("Installer"),
        "2": qsTr("Online fix")
    })
    readonly property var sizeFilterLabels: ({
        "0": qsTr("Any size"),
        "1": qsTr("< 1 GB"),
        "2": qsTr("1–5 GB"),
        "3": qsTr("5–20 GB"),
        "4": qsTr("20+ GB")
    })
    readonly property var recencyFilterLabels: ({
        "0": qsTr("Any time"),
        "1": qsTr("Last 7 days"),
        "2": qsTr("Last 30 days"),
        "3": qsTr("Last 90 days"),
        "4": qsTr("Last year")
    })

    property string searchQuery: ""
    property real savedGridScrollY: 0
    property real savedListScrollY: 0
    property bool restoringFilters: false

    signal openGame(string entryId)
    signal openSettings()
    signal addSourceRequested()

    Settings {
        id: catalogPrefs
        category: "catalog"
        property int sortMode: 0
        property int viewMode: 0
        property int typeFilter: -1
        property int sizeFilter: 0
        property int recencyFilter: 0
        property bool hasAddonsFilter: false
        property string genreFilter: ""
    }

    function applySortMode(mode) {
        if (Core.catalog.sortMode === mode)
            return
        catalogPrefs.sortMode = mode
        Core.catalog.sortMode = mode
    }

    function persistCatalogFilters() {
        if (root.restoringFilters)
            return
        catalogPrefs.typeFilter = Core.catalogTypeFilter
        catalogPrefs.sizeFilter = Core.catalogSizeFilter
        catalogPrefs.recencyFilter = Core.catalogRecencyFilter
        catalogPrefs.hasAddonsFilter = Core.catalogHasAddonsFilter
        catalogPrefs.genreFilter = Core.catalogGenreFilter
    }

    function restoreCatalogFilters() {
        root.restoringFilters = true
        Core.setCatalogFilters(catalogPrefs.typeFilter, catalogPrefs.sizeFilter,
                               catalogPrefs.recencyFilter, catalogPrefs.hasAddonsFilter,
                               catalogPrefs.genreFilter)
        root.restoringFilters = false
    }

    function ensureValidSource() {
        // Drops disabled chips and auto-selects the first enabled source if none left.
        Core.pruneDisabledCatalogSources()
    }

    function openFilterSheet() {
        catalogFilterSheet.openSheet()
    }

    function resetScroll() {
        grid.contentY = 0
        list.contentY = 0
    }

    function saveAndResetScroll() {
        // Save current scroll position of the view that's currently visible
        if (!root.listViewMode) {
            // Currently in grid mode, save grid scroll
            root.savedGridScrollY = grid.contentY
        } else {
            // Currently in list mode, save list scroll
            root.savedListScrollY = list.contentY
        }
    }

    function restoreScroll() {
        // Restore scroll position to the view that's now visible
        // Use a longer delay to ensure the view has fully updated
        Qt.callLater(function() {
            Qt.callLater(function() {
                if (root.listViewMode) {
                    // Switched to list mode, restore list scroll
                    list.contentY = root.savedListScrollY
                } else {
                    // Switched to grid mode, restore grid scroll
                    grid.contentY = root.savedGridScrollY
                }
            })
        })
    }

    function applyCatalogSearch(query) {
        if (!Core.activeCatalogSourceIds.length)
            return
        root.searchQuery = query
        Core.applyCatalogSearch(query)
        root.resetScroll()
    }

    Component.onCompleted: {
        Core.catalog.sortMode = catalogPrefs.sortMode
        root.restoreCatalogFilters()
        Core.prefetchCatalogCounts()
        root.ensureValidSource()
    }

    onEnabledChanged: {
        if (enabled)
            root.ensureValidSource()
    }

    Connections {
        target: Core
        function onActiveCatalogSourceIdsChanged() {
            root.searchQuery = ""
            catalogSearch.searchText = ""
            root.resetScroll()
        }
        function onCatalogFiltersChanged() {
            root.persistCatalogFilters()
        }
    }

    Connections {
        target: Core.sources
        function onSourcesChanged() {
            root.ensureValidSource()
        }
    }

    onListViewModeChanged: {
        saveAndResetScroll()
        restoreScroll()
    }

    CatalogFilterSheet {
        id: catalogFilterSheet
        sortOptions: root.sortOptions
        onSortApplied: function (mode) { root.applySortMode(mode) }
    }

    Component {
        id: catalogIntroHeaderComponent

        ColumnLayout {
            spacing: MD.Token.spacing.small

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                MD.Label {
                    text: qsTr("Catalog")
                    typescale: MD.Token.typescale.headline_medium
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: Messages.catalogPipelineDesc
                    wrapMode: Text.WordWrap
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }
            }

            CatalogSourceChips {
                Layout.fillWidth: true
            }

            Item {
                Layout.preferredHeight: MD.Token.spacing.extra_small
            }
        }
    }

    Loader {
        id: catalogIntroHeightProbe
        visible: false
        width: Math.max(0, root.width - pageMargin * 2)
        sourceComponent: catalogIntroHeaderComponent
    }

    readonly property real catalogIntroHeaderHeight: catalogIntroHeightProbe.item
                                                     ? catalogIntroHeightProbe.item.implicitHeight
                                                     : 160

    Item {
        anchors.fill: parent
        visible: !root.noSources

        Item {
            id: catalogStickyTop
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: pageMargin
            anchors.rightMargin: pageMargin
            anchors.topMargin: MD.Token.spacing.medium
            height: catalogStickyCol.implicitHeight + MD.Token.spacing.small
            z: 4
            layer.enabled: true

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: -MD.Token.spacing.medium
                anchors.leftMargin: -pageMargin
                anchors.rightMargin: -pageMargin
                color: MD.Token.color.surface
            }

            ColumnLayout {
                id: catalogStickyCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: MD.Token.spacing.small

                MD.SearchBar {
                    id: catalogSearch
                    Layout.fillWidth: true
                    Layout.maximumWidth: 560
                    Layout.alignment: Qt.AlignLeft
                    onAccepted: root.applyCatalogSearch(searchText.trim())
                    onSearchTextChanged: {
                        if (!searchText.length)
                            root.applyCatalogSearch("")
                    }

                    Connections {
                        target: root
                        function onSearchQueryChanged() {
                            if (catalogSearch.searchText !== root.searchQuery)
                                catalogSearch.searchText = root.searchQuery
                        }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small
                    visible: Core.catalogActiveFilterCount > 0

                    MD.FilterChip {
                        visible: Core.catalogTypeFilter >= 0
                        text: (root.typeFilterLabels[String(Core.catalogTypeFilter)] || qsTr("Type"))
                              + " ×"
                        checked: true
                        onClicked: Core.catalogTypeFilter = -1
                    }

                    MD.FilterChip {
                        visible: Core.catalogSizeFilter > 0
                        text: (root.sizeFilterLabels[String(Core.catalogSizeFilter)] || qsTr("Size"))
                              + " ×"
                        checked: true
                        onClicked: Core.catalogSizeFilter = 0
                    }

                    MD.FilterChip {
                        visible: Core.catalogRecencyFilter > 0
                        text: (root.recencyFilterLabels[String(Core.catalogRecencyFilter)] || qsTr("Added"))
                              + " ×"
                        checked: true
                        onClicked: Core.catalogRecencyFilter = 0
                    }

                    MD.FilterChip {
                        visible: Core.catalogHasAddonsFilter
                        text: qsTr("Has add-ons") + " ×"
                        checked: true
                        onClicked: Core.catalogHasAddonsFilter = false
                    }

                    MD.FilterChip {
                        visible: Core.catalogGenreFilter.length > 0
                        text: Core.catalogGenreFilter + " ×"
                        checked: true
                        onClicked: Core.catalogGenreFilter = ""
                    }
                }
            }
        }

        Item {
            id: catalogScrollClip
            anchors.top: catalogStickyTop.bottom
            anchors.topMargin: MD.Token.spacing.small
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true

            GridView {
                id: grid
                anchors.fill: parent
                anchors.leftMargin: pageMargin
                anchors.rightMargin: pageMargin
                anchors.bottomMargin: MD.Token.spacing.medium
                visible: !root.listViewMode
                clip: true
                model: Core.catalog
                cellWidth: root.cellWidth
                cellHeight: root.cellHeight
                cacheBuffer: root.cellHeight * 2
                reuseItems: false
                boundsBehavior: Flickable.StopAtBounds
                pixelAligned: true

                header: Column {
                    width: grid.width
                    spacing: MD.Token.spacing.small

                    Loader {
                        width: parent.width
                        sourceComponent: catalogIntroHeaderComponent
                    }

                    CatalogScrollHeader {
                        contentWidth: grid.width
                        hasSelection: Core.activeCatalogSourceIds.length > 0
                        listViewMode: root.listViewMode
                        onFilterRequested: root.openFilterSheet()
                        onViewModeChangeRequested: function (mode) { catalogPrefs.viewMode = mode }
                        onRefreshRequested: Core.refreshSelectedCatalogs()
                    }
                }

                ScrollBar.vertical: MD.ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: CatalogGameCard {
                    width: root.cardWidth
                    height: root.cardHeight
                    onOpenDetails: function (id) { root.openGame(id) }
                }
            }

            ListView {
                id: list
                anchors.fill: parent
                anchors.leftMargin: pageMargin
                anchors.rightMargin: pageMargin
                anchors.bottomMargin: MD.Token.spacing.medium
                visible: root.listViewMode
                clip: true
                model: Core.catalog
                spacing: MD.Token.spacing.extra_small
                cacheBuffer: root.listRowHeight * 8
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds

                header: Column {
                    width: list.width
                    spacing: MD.Token.spacing.small

                    Loader {
                        width: parent.width
                        sourceComponent: catalogIntroHeaderComponent
                    }

                    CatalogScrollHeader {
                        contentWidth: list.width
                        hasSelection: Core.activeCatalogSourceIds.length > 0
                        listViewMode: root.listViewMode
                        onFilterRequested: root.openFilterSheet()
                        onViewModeChangeRequested: function (mode) { catalogPrefs.viewMode = mode }
                        onRefreshRequested: Core.refreshSelectedCatalogs()
                    }
                }

                ScrollBar.vertical: MD.ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: CatalogGameCard {
                    width: list.width
                    height: root.listRowHeight
                    compactRow: true
                    onOpenDetails: function (id) { root.openGame(id) }
                }
            }
        }

        Rectangle {
            id: compactBar
            anchors.top: catalogStickyTop.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: compactRow.implicitHeight + MD.Token.spacing.small * 2
            color: MD.Token.color.surface
            opacity: root.compactBarOpacity
            visible: opacity > 0.02
            z: 5
            layer.enabled: visible

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                propagateComposedEvents: true
                onWheel: function (wheel) { wheel.accepted = false }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: MD.Token.color.outline_variant
                opacity: 0.35
            }

            RowLayout {
                id: compactRow
                anchors.fill: parent
                anchors.leftMargin: pageMargin + MD.Token.spacing.small
                anchors.rightMargin: pageMargin + MD.Token.spacing.small
                spacing: MD.Token.spacing.small

                MD.Label {
                    text: qsTr("Catalog")
                    typescale: MD.Token.typescale.title_large
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: Core.catalogLoading
                        ? qsTr("Loading…")
                        : qsTr("Found: %1").arg(Core.catalog.count)
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_large
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Item {
                    Layout.preferredWidth: compactFilterBtn.implicitWidth
                    Layout.preferredHeight: compactFilterBtn.implicitHeight

                    MD.IconButton {
                        id: compactFilterBtn
                        anchors.centerIn: parent
                        mdState.type: Core.catalogActiveFilterCount > 0
                                      ? MD.Enum.IBtFilledTonal
                                      : MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.filter_list
                        onClicked: root.openFilterSheet()
                    }

                    Rectangle {
                        visible: Core.catalogActiveFilterCount > 0
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.rightMargin: 2
                        anchors.topMargin: 2
                        width: Math.max(18, compactBadgeLabel.implicitWidth + 6)
                        height: 18
                        radius: 9
                        color: MD.Token.color.error
                        z: 2

                        MD.Label {
                            id: compactBadgeLabel
                            anchors.centerIn: parent
                            text: Core.catalogActiveFilterCount
                            color: MD.Token.color.on_error
                            typescale: MD.Token.typescale.label_small
                        }
                    }
                }

                MD.IconButton {
                    mdState.type: root.listViewMode ? MD.Enum.IBtStandard : MD.Enum.IBtFilledTonal
                    icon.name: MD.Token.icon.grid_view
                    onClicked: catalogPrefs.viewMode = 0
                }

                MD.IconButton {
                    mdState.type: root.listViewMode ? MD.Enum.IBtFilledTonal : MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.view_list
                    onClicked: catalogPrefs.viewMode = 1
                }

                MD.IconButton {
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.refresh
                    enabled: !Core.catalogLoading && Core.activeCatalogSourceIds.length > 0
                    onClicked: Core.refreshSelectedCatalogs()
                }
            }
        }

        Item {
            anchors.fill: parent
            anchors.topMargin: 200
            visible: root.catalogEmpty && !Core.catalogLoading
            z: 1

            Item {
                id: emptyResultsCard
                anchors.centerIn: parent
                width: Math.min(parent.width - pageMargin * 2, 720)
                height: 220
                clip: true

                layer.enabled: true
                layer.effect: MD.RoundClip {
                    corners: MD.Util.corners(root.cardRadius)
                    size: Qt.vector2d(emptyResultsCard.width, emptyResultsCard.height)
                }

                Rectangle {
                    anchors.fill: parent
                    color: MD.Token.color.surface_container_high
                }

                SpiderWebMark {
                    width: 260
                    height: 260
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: -130
                    strokeColor: MD.Token.color.primary
                    strokeWidth: 2.5
                    opacity: 0.18
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: MD.Token.spacing.extra_large
                    anchors.rightMargin: MD.Token.spacing.extra_large + 80
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.noSourceSelected
                              ? qsTr("Select sources")
                              : qsTr("Nothing found")
                        typescale: MD.Token.typescale.title_large
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.noSourceSelected
                              ? Messages.catalogEnableChipsHint
                              : qsTr("Try another search or refresh the catalog.")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                        wrapMode: Text.WordWrap
                    }

                    MD.Button {
                        visible: !root.noSourceSelected
                        text: qsTr("Refresh")
                        icon.name: MD.Token.icon.refresh
                        mdState.type: MD.Enum.BtFilledTonal
                        onClicked: Core.refreshCatalog(Core.activeCatalogSourceId)
                    }
                }
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: root.noSources

        ColumnLayout {
            anchors.centerIn: parent
            spacing: MD.Token.spacing.medium
            width: Math.min(parent.width - pageMargin * 2, 420)

            SpiderWebMark {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 160
                Layout.preferredHeight: 160
                width: 160
                height: 160
                strokeColor: MD.Token.color.primary
                strokeWidth: 2.5
                opacity: 0.35
            }

            MD.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No games")
                typescale: MD.Token.typescale.title_large
            }

            MD.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: Messages.catalogConnectHint
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: MD.Token.spacing.small

                MD.Button {
                    text: qsTr("Add catalog")
                    icon.name: MD.Token.icon.add
                    mdState.type: MD.Enum.BtFilled
                    onClicked: root.addSourceRequested()
                }

                MD.Button {
                    text: qsTr("Settings")
                    icon.name: MD.Token.icon.settings
                    mdState.type: MD.Enum.BtOutlined
                    onClicked: root.openSettings()
                }
            }
        }
    }
}
