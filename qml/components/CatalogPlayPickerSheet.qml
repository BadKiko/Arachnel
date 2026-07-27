import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal

    property int step: 0
    property int draftPlayMode: 0
    property string draftGenre: ""
    property int draftSize: 0

    readonly property var companyOptions: [
        { value: 0, label: qsTr("Surprise me") },
        { value: 1, label: qsTr("Solo") },
        { value: 2, label: qsTr("With friends") },
        { value: 3, label: qsTr("Online multiplayer") }
    ]

    readonly property var genreOptions: [
        "", "Action", "Adventure", "RPG", "Strategy", "Simulation", "Shooter",
        "Horror", "Indie", "Casual", "Sports", "Racing", "Puzzle", "Roguelike"
    ]

    readonly property var sizeOptions: [
        { value: 0, label: qsTr("Any size") },
        { value: 1, label: qsTr("< 1 GB") },
        { value: 2, label: qsTr("1–5 GB") },
        { value: 3, label: qsTr("5–20 GB") },
        { value: 4, label: qsTr("20+ GB") }
    ]

    signal applied()

    function openSheet() {
        step = 0
        draftPlayMode = 0
        draftGenre = ""
        draftSize = 0
        open()
    }

    function applyAndClose() {
        Core.applyCatalogPresentation(Core.catalog.sortMode, -1, draftSize, 0, false,
                                      draftGenre, draftPlayMode)
        root.applied()
        close()
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: qsTr("What to play?")
            typescale: MD.Token.typescale.headline_medium
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: root.step === 0 ? qsTr("Who are you playing with?")
                  : root.step === 1 ? qsTr("Pick a vibe")
                                    : qsTr("How much disk space?")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small
            visible: root.step === 0

            Repeater {
                model: root.companyOptions
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

        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small
            visible: root.step === 1

            Repeater {
                model: root.genreOptions
                MD.FilterChip {
                    required property string modelData
                    text: modelData.length === 0 ? qsTr("Any genre") : modelData
                    checkable: false
                    checked: root.draftGenre === modelData
                    elevated: root.draftGenre !== modelData
                    onClicked: root.draftGenre = modelData
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small
            visible: root.step === 2

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

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtText
                text: root.step === 0 ? qsTr("Cancel") : qsTr("Back")
                onClicked: {
                    if (root.step === 0)
                        root.close()
                    else
                        root.step -= 1
                }
            }

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtFilled
                text: root.step < 2 ? qsTr("Next") : qsTr("Show games")
                onClicked: {
                    if (root.step < 2)
                        root.step += 1
                    else
                        root.applyAndClose()
                }
            }
        }
    }
}
