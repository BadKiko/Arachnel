import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property bool applying: false

    readonly property var languageOptions: [
        { code: "en", label: qsTr("English") },
        { code: "ru", label: qsTr("Russian") }
    ]

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
            text: qsTr("Material 3 theme and palette apply across the app.")
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
                text: MD.Token.isDarkTheme ? qsTr("Dark theme") : qsTr("Light theme")
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
            text: qsTr("Palette")
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

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: qsTr("Language")
            typescale: MD.Token.typescale.label_large
        }

        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.small

            Repeater {
                model: root.languageOptions

                MD.InputChip {
                    required property var modelData

                    action: MD.Action {
                        checkable: true
                        checked: Core.settings.uiLanguage === modelData.code
                        text: modelData.label
                        onTriggered: Core.settings.uiLanguage = modelData.code
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Community translations")
                typescale: MD.Token.typescale.title_small
            }

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Missing your language? Help translate Arachnel on Weblate or send a pull request with translations/*.ts files.")
                wrapMode: Text.WordWrap
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_small
            }

            MD.Button {
                text: qsTr("Help translate")
                icon.name: MD.Token.icon.language
                mdState.type: MD.Enum.BtText
                onClicked: Core.openExternalUrl("https://hosted.weblate.org/")
            }
        }
    }
}
