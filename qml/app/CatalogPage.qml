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
    readonly property real scrollContentY: root.listViewMode ? catalogContent.listContentY : catalogContent.gridContentY
    readonly property real compactRevealStart: catalogContent.catalogIntroHeaderHeight
                                              + MD.Token.spacing.small
    readonly property real compactBarOpacity: {
        if (!catalogContent.visible)
            return 0
        const y = catalogContent.currentContentY
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
        organization: "Arachnel"
        application: "Arachnel"
        category: "catalog"
        property int sortMode: 0
        property int viewMode: 0
        property int typeFilter: -1
        property int sizeFilter: 0
        property int recencyFilter: 0
        property bool hasAddonsFilter: false
        property string genreFilter: ""
        property int playModeFilter: 0
    }

    function applySortMode(mode) {
        // Persist only — Core.applyCatalogPresentation already applied sort quietly.
        catalogPrefs.sortMode = mode
        if (Core.catalog.sortMode !== mode)
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
        catalogPrefs.playModeFilter = Core.catalogPlayModeFilter
    }

    function restoreCatalogFilters() {
        root.restoringFilters = true
        Core.setCatalogFilters(catalogPrefs.typeFilter, catalogPrefs.sizeFilter,
                               catalogPrefs.recencyFilter, catalogPrefs.hasAddonsFilter,
                               catalogPrefs.genreFilter, catalogPrefs.playModeFilter)
        root.restoringFilters = false
    }

    function ensureValidSource() {
        // Drops disabled chips and auto-selects the first enabled source if none left.
        Core.pruneDisabledCatalogSources()
    }

    function openFilterSheet() {
        catalogFilterSheet.openSheet()
    }

    function resetScroll() { catalogContent.resetScroll() }

    function saveAndResetScroll() {
        // Save current scroll position of the view that's currently visible
        if (!root.listViewMode) {
            // Currently in grid mode, save grid scroll
            root.savedGridScrollY = catalogContent.gridContentY
        } else {
            // Currently in list mode, save list scroll
            root.savedListScrollY = catalogContent.listContentY
        }
    }

    function restoreScroll() {
        // Restore scroll position to the view that's now visible
        // Use a longer delay to ensure the view has fully updated
        Qt.callLater(function() {
            Qt.callLater(function() {
                if (root.listViewMode) {
                    // Switched to list mode, restore list scroll
                    catalogContent.listContentY = root.savedListScrollY
                } else {
                    // Switched to grid mode, restore grid scroll
                    catalogContent.gridContentY = root.savedGridScrollY
                }
            })
        })
    }

    function applyCatalogSearch(query) {
        if (!Core.activeCatalogSourceIds.length)
            return
        root.searchQuery = query
        Core.applyCatalogSearch(query)
        catalogContent.resetScroll()
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
            catalogContent.searchText = ""
            catalogContent.resetScroll()
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
        onSortApplied: function (mode) { catalogPrefs.sortMode = mode }
    }


    CatalogPageContent {
        id: catalogContent
        anchors.fill: parent
        page: root
        prefs: catalogPrefs
    }
}
