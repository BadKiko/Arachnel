import QtQuick

import Qcm.Material as MD

// A round country flag for a language code. Flags are bundled SVGs (emoji flags do not render on
// Windows); a code without one falls back to a globe.
Item {
    id: root

    property string code: "en"
    property int diameter: 28

    // Language code -> country of the flag shown for it.
    readonly property string country: ({ en: "gb", uk: "ua", ja: "jp", zh: "cn", pt: "br" })[root.code] ?? root.code

    implicitWidth: diameter
    implicitHeight: diameter

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: MD.Token.color.surface_container_highest
        visible: flagImage.status !== Image.Ready
    }

    MD.Icon {
        anchors.centerIn: parent
        name: MD.Token.icon.language
        size: Math.round(root.diameter * 0.7)
        color: MD.Token.color.on_surface_variant
        visible: flagImage.status !== Image.Ready
    }

    Image {
        id: flagImage
        anchors.fill: parent
        source: "qrc:/flags/" + root.country + ".svg"
        sourceSize: Qt.size(root.diameter * 3, root.diameter * 2)
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
        visible: status === Image.Ready

        layer.enabled: visible
        layer.effect: MD.RoundClip {
            corners: MD.Util.corners(root.diameter / 2)
            size: Qt.vector2d(root.diameter, root.diameter)
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: 1
        border.color: MD.Util.transparent(MD.Token.color.on_surface, 0.18)
    }
}
