import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal

    property int draftSortMode: 0
    property int draftType: -1
    property int draftSize: 0
    property int draftRecency: 0
    property bool draftHasAddons: false
    property string draftGenre: ""

    property var sortOptions: []

    signal sortApplied(int mode)

    readonly property var typeOptions: [
        { value: -1, label: qsTr("All") },
        { value: 0, label: qsTr("Portable") },
        { value: 1, label: qsTr("Installer") },
        { value: 2, label: qsTr("Online fix") }
    ]

    readonly property var sizeOptions: [
        { value: 0, label: qsTr("Any") },
        { value: 1, label: qsTr("< 1 GB") },
        { value: 2, label: qsTr("1–5 GB") },
        { value: 3, label: qsTr("5–20 GB") },
        { value: 4, label: qsTr("20+ GB") }
    ]

    readonly property var recencyOptions: [
        { value: 0, label: qsTr("Any") },
        { value: 1, label: qsTr("Last 7 days") },
        { value: 2, label: qsTr("Last 30 days") },
        { value: 3, label: qsTr("Last 90 days") },
        { value: 4, label: qsTr("Last year") }
    ]

    function openSheet() {
        draftSortMode = Core.catalog.sortMode
        draftType = Core.catalogTypeFilter
        draftSize = Core.catalogSizeFilter
        draftRecency = Core.catalogRecencyFilter
        draftHasAddons = Core.catalogHasAddonsFilter
        draftGenre = Core.catalogGenreFilter
        open()
    }

    function applyAndClose() {
        Core.applyCatalogPresentation(draftSortMode, draftType, draftSize, draftRecency,
                                      draftHasAddons, draftGenre)
        root.sortApplied(draftSortMode)
        close()
    }

    function clearDraft() {
        draftSortMode = 0
        draftType = -1
        draftSize = 0
        draftRecency = 0
        draftHasAddons = false
        draftGenre = ""
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
            text: qsTr("Sort & filters")
            typescale: MD.Token.typescale.headline_medium
        }

        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentCol.implicitHeight, 420)
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
                        text: qsTr("Sort")
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.primary
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        Repeater {
                            model: root.sortOptions

                            MD.FilterChip {
                                required property var modelData
                                text: modelData.label
                                checkable: false
                                checked: root.draftSortMode === modelData.mode
                                elevated: root.draftSortMode !== modelData.mode
                                onClicked: root.draftSortMode = modelData.mode
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
                        text: qsTr("Type")
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.primary
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        Repeater {
                            model: root.typeOptions

                            MD.FilterChip {
                                required property var modelData
                                text: modelData.label
                                checkable: false
                                checked: root.draftType === modelData.value
                                elevated: root.draftType !== modelData.value
                                onClicked: root.draftType = modelData.value
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
                    Layout.leftMargin: MD.Token.spacing.large
                    Layout.rightMargin: MD.Token.spacing.large
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

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: MD.Token.spacing.large
                    Layout.rightMargin: MD.Token.spacing.large
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        text: qsTr("Extras")
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.primary
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

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: MD.Token.spacing.large
                    Layout.rightMargin: MD.Token.spacing.large
                    spacing: MD.Token.spacing.small
                    visible: Core.availableCatalogGenres.length > 0

                    MD.Label {
                        text: qsTr("Genre")
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.primary
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.FilterChip {
                            text: qsTr("Any")
                            checkable: false
                            checked: root.draftGenre.length === 0
                            elevated: root.draftGenre.length > 0
                            onClicked: root.draftGenre = ""
                        }

                        Repeater {
                            model: Core.availableCatalogGenres

                            MD.FilterChip {
                                required property string modelData
                                text: modelData
                                checkable: false
                                checked: root.draftGenre === modelData
                                elevated: root.draftGenre !== modelData
                                onClicked: root.draftGenre = modelData
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
