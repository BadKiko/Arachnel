import QtQuick

import Qcm.Material as MD

// A round action of the Discover feed with a caption under it. Pops a little when used.
Column {
    id: root

    property string iconName: ""
    property string label: ""
    property bool filled: false
    property bool accent: false

    signal triggered()

    spacing: 4

    Rectangle {
        id: bubble
        anchors.horizontalCenter: parent.horizontalCenter
        width: 56
        height: 56
        radius: hover.hovered ? 18 : 28
        color: root.accent ? MD.Token.color.primary : Qt.rgba(0, 0, 0, 0.45)
        border.width: root.accent ? 0 : 1
        border.color: Qt.rgba(1, 1, 1, 0.18)
        scale: tap.pressed ? 0.86 : 1

        Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: AppMotion.short } }
        Behavior on scale { NumberAnimation { duration: AppMotion.short; easing.type: Easing.OutBack; easing.overshoot: 3 } }

        MD.Icon {
            anchors.centerIn: parent
            name: root.iconName
            fill: root.filled
            size: 26
            color: root.accent ? MD.Token.color.on_primary : "white"
        }

        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler { id: tap; onTapped: root.triggered() }
    }

    MD.Label {
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        color: "white"
        typescale: MD.Token.typescale.label_medium
    }
}
