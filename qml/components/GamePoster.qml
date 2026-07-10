import QtQuick

import Qcm.Material as MD

Item {
    id: root

    property url source
    property string fallbackText: ""
    property int cornerRadius: MD.Token.shape.corner.large
    property bool hovered: mouseArea.containsMouse

    signal clicked()

    implicitWidth: 168
    implicitHeight: Math.round(width * 4 / 3)

    MD.Image {
        id: coverImage
        anchors.fill: parent
        source: root.source
        radius: root.cornerRadius
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        sourceSize.width: Math.min(240, Math.max(1, Math.round(width)))
        sourceSize.height: Math.min(320, Math.max(1, Math.round(height)))
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        visible: coverImage.status !== Image.Ready
        color: MD.Token.color.surface_container_highest

        MD.Label {
            anchors.centerIn: parent
            text: root.fallbackText
            typescale: MD.Token.typescale.headline_medium
            color: MD.Token.color.on_surface_variant
            opacity: 0.45
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        visible: root.hovered
        color: MD.Util.transparent(MD.Token.color.scrim, 0.22)
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
