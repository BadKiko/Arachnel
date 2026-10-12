import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Templates as T

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property string stepId
    readonly property bool isStepVisible: visible

    Layout.fillWidth: true
    Layout.leftMargin: MD.Token.spacing.large
    Layout.rightMargin: MD.Token.spacing.large
    spacing: MD.Token.spacing.small
    visible: root.stepId === "welcome" || root.stepId === "language" || root.stepId === "content"
             || root.stepId === "theme"

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.stepId === "welcome"
        spacing: MD.Token.spacing.small

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("A quick setup before you start")
            typescale: MD.Token.typescale.headline_small
            wrapMode: Text.WordWrap
        }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("A quick setup: language, storage, plugins, and a few defaults. Change anything later in Settings.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.stepId === "language"
        spacing: MD.Token.spacing.small

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Language")
            typescale: MD.Token.typescale.headline_small
        }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Choose the interface language.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
        Repeater {
            model: [
                { code: "en", label: "English" },
                { code: "ru", label: "Русский" }
            ]
            MD.Button {
                required property var modelData
                Layout.fillWidth: true
                text: modelData.label
                mdState.type: Core.settings.uiLanguage === modelData.code
                              ? MD.Enum.BtFilled : MD.Enum.BtOutlined
                onClicked: Core.settings.uiLanguage = modelData.code
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.stepId === "content"
        spacing: MD.Token.spacing.small

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Adult games")
            typescale: MD.Token.typescale.headline_small
        }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Some games contain nudity or sexual content. They are recognised by Steam's content labels and clear 18+ markers in the title, not by a word like \"sex\" alone. You can change this any time in the catalog filters.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
        OnboardingChoiceCard {
            Layout.topMargin: MD.Token.spacing.small
            iconName: MD.Token.icon.visibility_off
            title: qsTr("Hide adult games")
            description: qsTr("Keep them out of the catalog, search and Discover.")
            selected: !Core.settings.showAdultContent
            onClicked: Core.settings.showAdultContent = false
        }
        OnboardingChoiceCard {
            iconName: MD.Token.icon.visibility
            title: qsTr("Show adult games")
            description: qsTr("Show everything, including 18+ games.")
            selected: Core.settings.showAdultContent
            onClicked: Core.settings.showAdultContent = true
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.stepId === "theme"
        spacing: MD.Token.spacing.small

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Appearance")
            typescale: MD.Token.typescale.headline_small
        }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Pick light or dark theme, palette, and accent color. Change later in Settings.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small
            MD.Button {
                Layout.fillWidth: true
                text: qsTr("Dark")
                mdState.type: Appearance.themeMode === MD.Enum.Dark ? MD.Enum.BtFilled : MD.Enum.BtOutlined
                onClicked: Appearance.setThemeMode(MD.Enum.Dark)
            }
            MD.Button {
                Layout.fillWidth: true
                text: qsTr("Light")
                mdState.type: Appearance.themeMode === MD.Enum.Light ? MD.Enum.BtFilled : MD.Enum.BtOutlined
                onClicked: Appearance.setThemeMode(MD.Enum.Light)
            }
        }
        MD.Label {
            Layout.fillWidth: true
            Layout.topMargin: MD.Token.spacing.extra_small
            text: qsTr("Palette")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }
        MD.HorizontalListView {
            id: paletteList
            Layout.fillWidth: true
            expand: true
            spacing: MD.Token.spacing.small
            implicitHeight: 40
            model: MD.PaletteModel {}
            currentIndex: Appearance.paletteType
            MD.ActionGroup { id: paletteGroup }
            delegate: MD.InputChip {
                required property int index
                required property var model
                action: MD.Action {
                    T.ActionGroup.group: paletteGroup
                    icon.name: ""
                    checkable: true
                    checked: paletteList.currentIndex === index
                    text: model.name
                    onTriggered: {
                        paletteList.currentIndex = index
                        Appearance.setPaletteType(index)
                    }
                }
            }
        }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Primary")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }
        Grid {
            Layout.alignment: Qt.AlignHCenter
            spacing: MD.Token.spacing.medium
            rows: 2
            columns: 6
            Repeater {
                model: AccentColors.palette
                MD.ColorRadio {
                    required property var modelData
                    size: 32
                    color: modelData.color
                    checked: Appearance.accentColor === modelData.color
                    onClicked: Appearance.setAccentColor(modelData.color)
                }
            }
        }
    }
}
