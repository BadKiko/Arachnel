import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal

    property int draftSize: 0
    property int draftRecency: 0
    property bool draftHasAddons: false
    property bool draftShowAdult: false
    property string draftGenre: ""
    property int draftPlayMode: 0
    property var draftHiddenSources: []
    property string genreSearch: ""

    readonly property var playModeOptions: [
        { value: 0, label: qsTr("Any") },
        { value: 1, label: qsTr("Single-player") },
        { value: 2, label: qsTr("Multiplayer") }
    ]

    readonly property var sizeOptions: [
        { value: 0, label: qsTr("Any") },
        { value: 1, label: qsTr("< 1 GB") },
        { value: 2, label: qsTr("1-5 GB") },
        { value: 3, label: qsTr("5-20 GB") },
        { value: 4, label: qsTr("20+ GB") }
    ]

    readonly property var recencyOptions: [
        { value: 0, label: qsTr("Any") },
        { value: 1, label: qsTr("7 days") },
        { value: 2, label: qsTr("30 days") },
        { value: 3, label: qsTr("90 days") },
        { value: 4, label: qsTr("1 year") }
    ]

    readonly property bool draftActive: draftSize > 0 || draftRecency > 0 || draftHasAddons
                                        || draftGenre.length > 0 || draftPlayMode > 0
                                        || draftHiddenSources.length > 0

    function labelsOf(options) {
        return options.map(function (o) { return o.label })
    }

    readonly property var visibleGenres: {
        const needle = root.genreSearch.trim().toLowerCase()
        const all = Core.availableCatalogGenres
        if (!needle.length)
            return all
        const filtered = []
        for (let i = 0; i < all.length; ++i) {
            const key = String(all[i])
            const label = String(Core.catalogGenreLabel(key))
            if (key.toLowerCase().indexOf(needle) >= 0 || label.toLowerCase().indexOf(needle) >= 0)
                filtered.push(key)
        }
        return filtered
    }

    function genreIcon(key) {
        switch (key) {
        case "Action":
            return MD.Token.icon.sports_esports
        case "Adventure":
            return MD.Token.icon.explore
        case "RPG":
            return MD.Token.icon.swords
        case "Strategy":
            return MD.Token.icon.psychology
        case "Simulation":
            return MD.Token.icon.precision_manufacturing
        case "Shooter":
            return MD.Token.icon.ads_click
        case "Horror":
            return MD.Token.icon.dark_mode
        case "Indie":
            return MD.Token.icon.extension
        case "Casual":
            return MD.Token.icon.mood
        case "Sports":
            return MD.Token.icon.sports
        case "Racing":
            return MD.Token.icon.directions_car
        case "Puzzle":
            return MD.Token.icon.grid_4x4
        case "Platformer":
            return MD.Token.icon.stairs
        case "Fighting":
            return MD.Token.icon.sports_mma
        case "Survival":
            return MD.Token.icon.forest
        case "Open World":
            return MD.Token.icon.landscape
        case "Roguelike":
            return MD.Token.icon.replay
        case "Visual Novel":
            return MD.Token.icon.menu_book
        case "Card":
            return MD.Token.icon.style
        case "Early Access":
            return MD.Token.icon.rocket_launch
        case "Free to Play":
            return MD.Token.icon.money_off
        case "Massively Multiplayer":
            return MD.Token.icon["public"]
        case "VR":
            return MD.Token.icon.view_in_ar
        default:
            return MD.Token.icon.sports_esports
        }
    }

    function toggleSource(sourceId) {
        const next = root.draftHiddenSources.slice()
        const at = next.indexOf(sourceId)
        if (at >= 0)
            next.splice(at, 1)
        else
            next.push(sourceId)
        root.draftHiddenSources = next
    }

    function openSheet() {
        draftSize = Core.catalogSizeFilter
        draftRecency = Core.catalogRecencyFilter
        draftHasAddons = Core.catalogHasAddonsFilter
        draftShowAdult = Core.settings.showAdultContent
        draftGenre = Core.catalogGenreFilter
        draftPlayMode = Core.catalogPlayModeFilter
        draftHiddenSources = Core.hiddenCatalogSourceIds.slice()
        genreSearch = ""
        open()
    }

    function applyAndClose() {
        Core.setHiddenCatalogSourceIds(root.draftHiddenSources)
        Core.settings.showAdultContent = root.draftShowAdult
        Core.applyCatalogPresentation(Core.catalog.sortMode, -1, draftSize, draftRecency,
                                      draftHasAddons, draftGenre, draftPlayMode)
        close()
    }

    function clearDraft() {
        draftSize = 0
        draftRecency = 0
        draftHasAddons = false
        draftGenre = ""
        draftPlayMode = 0
        draftHiddenSources = []
        genreSearch = ""
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.medium
            Layout.topMargin: MD.Token.spacing.small
            Layout.bottomMargin: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Filters")
                typescale: MD.Token.typescale.headline_small
            }

            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("Clear all")
                enabled: root.draftActive
                onClicked: root.clearDraft()
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentCol.implicitHeight, 560)
            contentWidth: width
            contentHeight: contentCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: contentCol
                width: parent.width
                spacing: MD.Token.spacing.large

                CatalogFilterSection {
                    title: qsTr("Players")
                    iconName: MD.Token.icon.group
                    modified: root.draftPlayMode > 0

                    ConnectedTextToggle {
                        labels: root.labelsOf(root.playModeOptions)
                        currentIndex: root.draftPlayMode
                        onPicked: function (index) { root.draftPlayMode = index }
                    }
                }

                CatalogFilterSection {
                    title: qsTr("Genre")
                    iconName: MD.Token.icon.category
                    modified: root.draftGenre.length > 0

                    AppTextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search genres")
                        text: root.genreSearch
                        leadingIcon: MD.Token.icon.search
                        onTextEdited: root.genreSearch = text
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        CatalogGenreTile {
                            visible: root.genreSearch.trim().length === 0
                            genreKey: ""
                            selected: root.draftGenre.length === 0
                            iconName: MD.Token.icon.apps
                            onClicked: root.draftGenre = ""
                        }

                        Repeater {
                            model: root.visibleGenres

                            CatalogGenreTile {
                                required property string modelData
                                genreKey: modelData
                                selected: root.draftGenre === modelData
                                iconName: root.genreIcon(modelData)
                                onClicked: root.draftGenre = modelData
                            }
                        }
                    }
                }

                CatalogFilterSection {
                    title: qsTr("Size")
                    iconName: MD.Token.icon.hard_drive
                    modified: root.draftSize > 0

                    ConnectedTextToggle {
                        labels: root.labelsOf(root.sizeOptions)
                        currentIndex: root.draftSize
                        onPicked: function (index) { root.draftSize = index }
                    }
                }

                CatalogFilterSection {
                    title: qsTr("Added")
                    iconName: MD.Token.icon.schedule
                    modified: root.draftRecency > 0

                    ConnectedTextToggle {
                        labels: root.labelsOf(root.recencyOptions)
                        currentIndex: root.draftRecency
                        onPicked: function (index) { root.draftRecency = index }
                    }
                }

                CatalogFilterSection {
                    visible: Core.sources.enabledCount > 0
                    title: qsTr("Source")
                    iconName: MD.Token.icon.storefront
                    modified: root.draftHiddenSources.length > 0

                    CatalogSourceChips {
                        Layout.fillWidth: true
                        hiddenIds: root.draftHiddenSources
                        onSourceToggled: function (sourceId) {
                            root.toggleSource(sourceId)
                        }
                    }
                }

                CatalogFilterSection {
                    title: qsTr("Content")
                    iconName: MD.Token.icon.explicit

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            MD.Label {
                                Layout.fillWidth: true
                                text: qsTr("Show adult games")
                                typescale: MD.Token.typescale.body_large
                            }
                            MD.Label {
                                Layout.fillWidth: true
                                text: qsTr("Games with nudity or sexual content (Steam labels, 18+ markers in the title).")
                                typescale: MD.Token.typescale.body_small
                                color: MD.Token.color.on_surface_variant
                                wrapMode: Text.WordWrap
                            }
                        }

                        MD.Switch {
                            checked: root.draftShowAdult
                            onToggled: root.draftShowAdult = checked
                        }
                    }
                }

                CatalogFilterSection {
                    title: qsTr("Add-ons")
                    iconName: MD.Token.icon.extension
                    modified: root.draftHasAddons

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Only games with add-ons")
                            typescale: MD.Token.typescale.body_large
                            color: MD.Token.color.on_surface_variant
                            wrapMode: Text.WordWrap
                        }

                        MD.Switch {
                            checked: root.draftHasAddons
                            onToggled: root.draftHasAddons = checked
                        }
                    }
                }

                Item {
                    Layout.preferredHeight: MD.Token.spacing.small
                }
            }
        }

        MD.Button {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            Layout.bottomMargin: MD.Token.spacing.medium
            mdState.type: MD.Enum.BtFilled
            text: qsTr("Apply")
            onClicked: root.applyAndClose()
        }
    }
}
