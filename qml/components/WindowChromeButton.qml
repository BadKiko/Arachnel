import QtQuick

import Qcm.Material as MD

Item {
    id: root

    property string iconName
    property bool danger: false
    property bool forceHovered: false
    property bool forcePressed: false
    readonly property bool hot: mouseArea.containsMouse || forceHovered
    readonly property bool down: mouseArea.pressed || forcePressed

    signal clicked()

    implicitWidth: 40
    implicitHeight: 32
    width: implicitWidth
    height: implicitHeight

    Rectangle {
        anchors.fill: parent
        color: {
            if (!root.hot && !root.down)
                return "transparent"
            if (root.danger)
                return "#e81123"
            return MD.Token.color.on_surface
        }
        opacity: root.danger ? 1 : (root.hot || root.down ? 0.08 : 0)
    }

    MD.Icon {
        anchors.centerIn: parent
        name: root.iconName
        size: 14
        color: root.danger && root.hot
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
