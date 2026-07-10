import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property bool applying: false

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    function syncFromStore() {
        applying = true
        Appearance.apply()
        paletteListView.currentIndex = Appearance.paletteType
        applying = false
    }

    Component.onCompleted: syncFromStore()

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: qsTr("Тема и палитра Material 3 применяются ко всему приложению.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.medium

            MD.Label {
                text: MD.Token.isDarkTheme ? qsTr("Тёмная тема") : qsTr("Светлая тема")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_large
            }

            MD.Switch {
                id: themeSwitch
                checked: MD.Token.isDarkTheme
                onToggled: {
                    if (root.applying)
                        return
                    Appearance.setThemeMode(checked ? MD.Enum.Dark : MD.Enum.Light)
                }
            }

            Item { Layout.fillWidth: true }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            text: qsTr("Палитра")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }

        MD.HorizontalListView {
            id: paletteListView
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
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
                        if (root.applying)
                            return
                        paletteListView.currentIndex = index
                        Appearance.setPaletteType(index)
                    }
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            text: qsTr("Primary")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }

        Grid {
            Layout.alignment: Qt.AlignHCenter
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.medium
            rows: 2
            columns: 5

            Repeater {
                model: AccentColors.palette

                MD.ColorRadio {
                    required property var modelData
                    size: 32
                    color: modelData.color
                    checked: Appearance.accentColor === modelData.color
                    onClicked: {
                        if (root.applying)
                            return
                        Appearance.setAccentColor(modelData.color)
                    }
                }
            }
        }
    }
}
