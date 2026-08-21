import QtQuick
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    property var sortOptions: []

    implicitWidth: btn.implicitWidth
    implicitHeight: btn.implicitHeight

    MD.IconButton {
        id: btn
        anchors.centerIn: parent
        mdState.type: MD.Enum.IBtStandard
        icon.name: MD.Token.icon.sort
        onClicked: sortMenu.open()

        MD.Menu {
            id: sortMenu
            y: parent.height
            autoClose: true

            Repeater {
                model: root.sortOptions

                MD.MenuItem {
                    required property var modelData
                    text: modelData.label
                    onTriggered: {
                        Core.catalog.sortMode = modelData.mode
                        root.sortModeChosen(modelData.mode)
                    }
                }
            }
        }
    }

    signal sortModeChosen(int mode)
}
