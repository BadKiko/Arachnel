import QtQuick

import Qcm.Material as MD

Item {
    id: root

    property string iconName
    property bool danger: false

    signal clicked()

    implicitWidth: 40
    implicitHeight: 32

    Rectangle {
        anchors.fill: parent
        color: {
            if (!mouseArea.containsMouse && !mouseArea.pressed)
                return "transparent"
            if (root.danger)
                return "#e81123"
            return MD.Token.color.on_surface
        }
        opacity: root.danger ? 1 : (mouseArea.containsMouse || mouseArea.pressed ? 0.08 : 0)
    }

    MD.Icon {
        anchors.centerIn: parent
        name: root.iconName
        size: 14
        color: root.danger && mouseArea.containsMouse
               ? "#ffffff"
               : MD.Token.color.on_surface_variant
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
