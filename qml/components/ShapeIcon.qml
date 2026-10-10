import QtQuick

import Qcm.Material as MD

// An icon inside an expressive shape. Under the pointer the shape turns a little and its lobes
// deepen, settling back with a springy overshoot.
Item {
    id: root

    property string iconName: ""
    property string kind: "cookie9"
    property color color: MD.Token.color.primary_container
    property color iconColor: MD.Token.color.on_primary_container
    property real iconSize: Math.round(Math.min(width, height) * 0.46)
    property bool interactive: true
    property bool active: false

    implicitWidth: 48
    implicitHeight: 48

    readonly property bool lively: (interactive && hover.hovered) || active

    ExpressiveShape {
        id: shape
        anchors.fill: parent
        kind: root.kind
        color: root.color
        morph: root.lively ? 1.35 : 1.0
        rotation: root.lively ? 22 : 0

        Behavior on morph {
            NumberAnimation { duration: AppMotion.scaled(420); easing.type: Easing.OutBack; easing.overshoot: 1.6 }
        }
        Behavior on rotation {
            NumberAnimation { duration: AppMotion.scaled(520); easing.type: Easing.OutBack; easing.overshoot: 1.4 }
        }
        Behavior on color { ColorAnimation { duration: AppMotion.short } }
    }

    MD.Icon {
        anchors.centerIn: parent
        name: root.iconName
        size: root.iconSize
        color: root.iconColor
        visible: root.iconName.length > 0
    }

    HoverHandler {
        id: hover
        enabled: root.interactive
    }
}
