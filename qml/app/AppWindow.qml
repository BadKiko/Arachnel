import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

MD.ApplicationWindow {
    id: root

    visible: true
    width: 1440
    height: 720
    title: qsTr("Arachnel")

    MD.MProp.textColor: MD.MProp.color.on_surface
    MD.MProp.backgroundColor: MD.MProp.color.background

    property int windowClass: MD.Token.window_class.select_type(width)
    MD.MProp.size.windowClass: windowClass
    onWidthChanged: windowClass = MD.Token.window_class.select_type(width)

    Component.onCompleted: Appearance.apply()

    MD.IconButton {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: MD.Token.spacing.medium
        mdState.type: MD.Enum.IBtStandard
        icon.name: MD.Token.icon.settings
        onClicked: settingsSheet.open()
    }

    HomePage {
        anchors.fill: parent
    }

    SettingsSheet {
        id: settingsSheet
        anchors.fill: parent
    }
}
