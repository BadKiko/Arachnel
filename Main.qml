import QtQuick
import QtQuick.Layouts
import QtCore

import Qcm.Material as MD

MD.ApplicationWindow {
    id: root
    visible: true
    width: 720
    height: 480
    title: qsTr("Arachnel")

    MD.MProp.textColor: MD.MProp.color.on_surface
    MD.MProp.backgroundColor: MD.MProp.color.background

    property int windowClass: MD.Token.window_class.select_type(width)
    MD.MProp.size.windowClass: windowClass
    onWidthChanged: windowClass = MD.Token.window_class.select_type(width)

    Settings {
        id: appearance
        category: "appearance"
        property int themeMode: MD.Enum.Dark
        property int paletteType: 2
        property string accentColor: "#984300"
    }

    function applyAppearance() {
        MD.Token.color.useSysColorSM = false
        MD.Token.color.useSysAccentColor = false
        MD.Token.themeMode = appearance.themeMode
        MD.Token.color.paletteType = appearance.paletteType
        MD.Token.color.accentColor = appearance.accentColor
    }

    Component.onCompleted: applyAppearance()

    MD.IconButton {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: MD.Token.spacing.medium
        mdState.type: MD.Enum.IBtStandard
        icon.name: MD.Token.icon.settings
        onClicked: settingsSheet.open()
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: MD.Token.spacing.large

        MD.Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Arachnel")
            typescale: MD.Token.typescale.headline_medium
            color: MD.Token.color.primary
        }

        MD.Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("QML Material 3")
            typescale: MD.Token.typescale.body_large
            color: MD.Token.color.on_surface_variant
        }

        MD.Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Нажми меня")
            mdState.type: MD.Enum.BtFilled
            mdState.backgroundColor: MD.Token.color.primary
            mdState.textColor: MD.Token.color.on_primary
            onClicked: console.log("Arachnel: button clicked")
        }
    }

    MD.BottomSheet {
        id: settingsSheet
        anchors.fill: parent
        sheetType: MD.Enum.BottomSheetModal

        onAboutToShow: settingsPage.syncFromStore()

        SettingsPage {
            id: settingsPage
            width: settingsSheet.sheetWidth
            appearance: appearance
            applyAppearance: root.applyAppearance
            closeSheet: function () { settingsSheet.close() }
        }
    }
}
