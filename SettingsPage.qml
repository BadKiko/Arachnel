import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var appearance
    required property var applyAppearance
    required property var closeSheet

    spacing: 0

    readonly property int contentMargin: MD.Token.spacing.large

    property bool applyingSavedAppearance: false

    readonly property var accentPalette: [
        { name: "Unicase", color: "#984300" },
        { name: "Red", color: "#F44336" },
        { name: "Pink", color: "#E91E63" },
        { name: "Purple", color: "#9C27B0" },
        { name: "Indigo", color: "#3F51B5" },
        { name: "Teal", color: "#009688" },
        { name: "LightGreen", color: "#8BC34A" },
        { name: "Yellow", color: "#FFEB3B" },
        { name: "Amber", color: "#FFC107" },
        { name: "Orange", color: "#FF9800" }
    ]

    function syncFromStore() {
        applyingSavedAppearance = true
        applyAppearance()
        paletteListView.currentIndex = appearance.paletteType
        applyingSavedAppearance = false
    }

    MD.DialogHeader {
        Layout.fillWidth: true
        title: qsTr("Внешний вид")
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: contentMargin
        Layout.rightMargin: contentMargin
        Layout.topMargin: MD.Token.spacing.small
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Тема и палитра Material 3 применяются ко всему приложению.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.medium

            MD.Label {
                text: MD.Token.isDarkTheme ? qsTr("Тёмная тема") : qsTr("Светлая тема")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_large
            }

            MD.Switch {
                checked: MD.Token.isDarkTheme
                onCheckedChanged: {
                    if (root.applyingSavedAppearance)
                        return
                    const mode = checked ? MD.Enum.Dark : MD.Enum.Light
                    appearance.themeMode = mode
                    MD.Token.themeMode = mode
                }
            }

            Item { Layout.fillWidth: true }
        }

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Палитра")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }

        MD.HorizontalListView {
            id: paletteListView
            Layout.fillWidth: true
            expand: true
            spacing: MD.Token.spacing.small
            implicitHeight: 40
            model: MD.PaletteModel {}

            MD.ActionGroup {
                id: paletteActionGroup
            }

            delegate: MD.InputChip {
                required property int index
                required property var model

                action: MD.Action {
                    T.ActionGroup.group: paletteActionGroup
                    icon.name: ""
                    checkable: true
                    checked: paletteListView.currentIndex === index
                    text: model.name
                    onTriggered: {
                        if (root.applyingSavedAppearance)
                            return
                        paletteListView.currentIndex = index
                        appearance.paletteType = index
                        MD.Token.color.paletteType = index
                    }
                }
            }
        }

        MD.AutoDivider {
            Layout.fillWidth: true
        }

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Primary")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }

        Grid {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: MD.Token.spacing.small
            spacing: MD.Token.spacing.medium
            rows: 2
            columns: 5

            Repeater {
                model: root.accentPalette

                MD.ColorRadio {
                    required property var modelData
                    size: 32
                    color: modelData.color
                    checked: appearance.accentColor === modelData.color
                    onClicked: {
                        if (root.applyingSavedAppearance)
                            return
                        appearance.accentColor = modelData.color
                        MD.Token.color.useSysAccentColor = false
                        MD.Token.color.accentColor = modelData.color
                    }
                }
            }
        }
    }

    MD.Divider {
        Layout.fillWidth: true
        Layout.topMargin: MD.Token.spacing.medium
        Layout.leftMargin: contentMargin
        Layout.rightMargin: contentMargin
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignRight
        Layout.rightMargin: contentMargin
        Layout.bottomMargin: MD.Token.spacing.large
        Layout.topMargin: MD.Token.spacing.small

        MD.Button {
            mdState.type: MD.Enum.BtText
            text: qsTr("Готово")
            onClicked: root.closeSheet()
        }
    }
}
