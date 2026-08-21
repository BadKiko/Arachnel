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
    property string draftGenre: ""
    property int draftPlayMode: 0
    property var draftHiddenSources: []
    property string genreSearch: ""
    property bool moreOpen: false

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
        { value: 1, label: qsTr("Last 7 days") },
        { value: 2, label: qsTr("Last 30 days") },
        { value: 3, label: qsTr("Last 90 days") },
        { value: 4, label: qsTr("Last year") }
    ]

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
            return MD.Token.icon.public
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
        draftGenre = Core.catalogGenreFilter
        draftPlayMode = Core.catalogPlayModeFilter
        draftHiddenSources = Core.hiddenCatalogSourceIds.slice()
        genreSearch = ""
        moreOpen = draftSize > 0 || draftRecency > 0 || draftHasAddons
                   || draftHiddenSources.length > 0
        open()
    }

    function applyAndClose() {
        Core.setHiddenCatalogSourceIds(root.draftHiddenSources)
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

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            Layout.bottomMargin: MD.Token.spacing.small
            text: qsTr("Filters")
            typescale: MD.Token.typescale.headline_medium
        }

        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentCol.implicitHeight, 620)
            contentWidth: width
            contentHeight: contentCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: contentCol
                width: parent.width
                spacing: MD.Token.spacing.medium

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: MD.Token.spacing.large
                    Layout.rightMargin: MD.Token.spacing.large
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        text: qsTr("Players")
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.primary
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        Repeater {
                            model: root.playModeOptions

                            MD.FilterChip {
                                required property var modelData
                                text: modelData.label
                                checkable: false
                                checked: root.draftPlayMode === modelData.value
                                elevated: root.draftPlayMode !== modelData.value
                                onClicked: root.draftPlayMode = modelData.value
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: MD.Token.spacing.large
                    Layout.rightMargin: MD.Token.spacing.large
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        text: qsTr("Genre")
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.primary
                    }

                    AppTextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search genres")
                        text: root.genreSearch
                        leadingIcon: MD.Token.icon.search
                        onTextEdited: root.genreSearch = text
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: Math.max(3, Math.floor(width / 140))
                        rowSpacing: MD.Token.spacing.small
                        columnSpacing: MD.Token.spacing.small

                        CatalogGenreTile {
                            Layout.fillWidth: true
                            genreKey: ""
                            selected: root.draftGenre.length === 0
                            iconName: MD.Token.icon.apps
                            onClicked: root.draftGenre = ""
                        }

                        Repeater {
                            model: root.visibleGenres

                            CatalogGenreTile {
                                required property string modelData
                                Layout.fillWidth: true
                                genreKey: modelData
                                selected: root.draftGenre === modelData
                                iconName: root.genreIcon(modelData)
                                onClicked: root.draftGenre = modelData
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: MD.Token.spacing.large
                    Layout.rightMargin: MD.Token.spacing.large
                    spacing: MD.Token.spacing.small

                    MD.Button {
                        mdState.type: MD.Enum.BtText
                        text: root.moreOpen ? qsTr("Less") : qsTr("More")
                        onClicked: root.moreOpen = !root.moreOpen
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.medium
                        visible: root.moreOpen

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small
                            visible: Core.sources.enabledCount > 0

                            MD.Label {
                                text: qsTr("Source")
                                typescale: MD.Token.typescale.label_large
                                color: MD.Token.color.primary
                            }

                            CatalogSourceChips {
                                Layout.fillWidth: true
                                hiddenIds: root.draftHiddenSources
                                onSourceToggled: function (sourceId) {
                                    root.toggleSource(sourceId)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small

                            MD.Label {
                                text: qsTr("Size")
                                typescale: MD.Token.typescale.label_large
                                color: MD.Token.color.primary
                            }

                            Flow {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.small

                                Repeater {
                                    model: root.sizeOptions

                                    MD.FilterChip {
                                        required property var modelData
                                        text: modelData.label
                                        checkable: false
                                        checked: root.draftSize === modelData.value
                                        elevated: root.draftSize !== modelData.value
                                        onClicked: root.draftSize = modelData.value
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small

                            MD.Label {
                                text: qsTr("Added")
                                typescale: MD.Token.typescale.label_large
                                color: MD.Token.color.primary
                            }

                            Flow {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.small

                                Repeater {
                                    model: root.recencyOptions

                                    MD.FilterChip {
                                        required property var modelData
                                        text: modelData.label
                                        checkable: false
                                        checked: root.draftRecency === modelData.value
                                        elevated: root.draftRecency !== modelData.value
                                        onClicked: root.draftRecency = modelData.value
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small

                            MD.Label {
                                Layout.fillWidth: true
                                text: qsTr("Has add-ons")
                                typescale: MD.Token.typescale.body_large
                            }

                            MD.Switch {
                                checked: root.draftHasAddons
                                onToggled: root.draftHasAddons = checked
                            }
                        }
                    }
                }

                Item {
                    Layout.preferredHeight: MD.Token.spacing.small
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtText
                text: qsTr("Clear all")
                onClicked: root.clearDraft()
            }

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtFilled
                text: qsTr("Apply")
                onClicked: root.applyAndClose()
            }
        }
    }
}
