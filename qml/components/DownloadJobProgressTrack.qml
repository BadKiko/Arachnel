import QtQuick
import Qcm.Material as MD

Item {
    id: root

    required property var page

    // Active / installing only — no track on completed (looks like a stray underline).
    readonly property bool showTrack: page.inProgress || page.isInstalling
    readonly property real fillRatio: {
        if (page.isInstalling)
            return 0
        return Math.max(0, Math.min(1, (page.progress || 0) / 100))
    }

    visible: showTrack
    implicitHeight: visible ? (page.addonRow ? 4 : 5) : 0
    clip: true

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: MD.Util.transparent(MD.Token.color.primary, 0.18)
    }

    Rectangle {
        id: installIndeterminate
        visible: page.isInstalling
        height: parent.height
        width: Math.max(28, parent.width * 0.28)
        radius: height / 2
        color: MD.Token.color.primary
        x: -width

        SequentialAnimation {
            running: page.isInstalling && root.visible
            loops: Animation.Infinite
            NumberAnimation {
                target: installIndeterminate
                property: "x"
                from: -installIndeterminate.width
                to: root.width
                duration: 1100
                easing.type: Easing.InOutCubic
            }
            PauseAnimation { duration: 80 }
        }
    }

    Rectangle {
        visible: !page.isInstalling
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        // No Behavior: model remounts / width flicker would re-animate 0→N every tick.
        width: parent.width * root.fillRatio
        radius: height / 2
        color: page.isPaused ? MD.Token.color.on_surface_variant
                             : (page.isFailed ? MD.Token.color.error : MD.Token.color.primary)
    }
}
