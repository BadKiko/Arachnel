import QtQuick

import Qcm.Material as MD

Item {
    id: root

    property bool checked: false

    signal toggled(bool checked)

    implicitWidth: 40
    implicitHeight: 40

    Rectangle {
        id: box
        anchors.centerIn: parent
        width: 18
        height: 18
        radius: 2
        color: root.checked ? MD.Token.color.primary : "transparent"
        border.width: 2
        border.color: root.checked
                    ? MD.Token.color.primary
                    : MD.Token.color.on_surface_variant

        MD.Icon {
            anchors.centerIn: parent
            visible: root.checked
            name: MD.Token.icon.check
            size: 14
            color: MD.Token.color.on_primary
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
