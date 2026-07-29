import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// Adaptive catalog index rail - date / letter / size / kind stops from Core.
Item {
    id: root

    signal stopSelected(int row, string label)

    property var stops: []
    property string activeLabel: ""

    readonly property int stopCount: Array.isArray(root.stops) ? root.stops.length : 0

    implicitWidth: 28
    width: Math.max(implicitWidth, MD.Token.spacing.large)
    visible: root.stopCount >= 2

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: MD.Token.shape.corner.large
        color: MD.Util.transparent(MD.Token.color.surface_container_high, 0.72)
    }

    Column {
        id: stopCol
        anchors.fill: parent
        anchors.topMargin: MD.Token.spacing.extra_small
        anchors.bottomMargin: MD.Token.spacing.extra_small
        anchors.leftMargin: 1
        anchors.rightMargin: 1

        Repeater {
            model: root.stops

            Item {
                required property var modelData
                required property int index

                width: stopCol.width
                height: Math.max(10, stopCol.height / Math.max(1, root.stopCount))

                MD.Label {
                    anchors.centerIn: parent
                    width: parent.width - 2
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData.label || ""
                    typescale: MD.Token.typescale.label_small
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    color: root.activeLabel === (modelData.label || "")
                           ? MD.Token.color.primary
                           : MD.Token.color.on_surface_variant
                    opacity: root.activeLabel === (modelData.label || "") ? 1 : 0.85
                }
            }
        }
    }

    Rectangle {
        visible: root.activeLabel.length > 0
        width: Math.max(64, previewLabel.implicitWidth + MD.Token.spacing.medium * 2)
        height: 64
        radius: MD.Token.shape.corner.large
        color: MD.Token.color.primary_container
        anchors.right: parent.left
        anchors.rightMargin: MD.Token.spacing.small
        anchors.verticalCenter: parent.verticalCenter
        z: 2

        MD.Label {
            id: previewLabel
            anchors.centerIn: parent
            text: root.activeLabel
            typescale: MD.Token.typescale.title_large
            color: MD.Token.color.on_primary_container
        }
    }

    function stopAtY(y) {
        if (root.stopCount === 0 || height <= 0)
            return null
        const idx = Math.max(0, Math.min(root.stopCount - 1,
                                         Math.floor(y / height * root.stopCount)))
        return root.stops[idx]
    }

    function selectAtY(y) {
        const stop = stopAtY(y)
        if (!stop || stop.row === undefined || stop.row < 0)
            return
        const label = stop.label || ""
        if (label === root.activeLabel)
            return
        root.activeLabel = label
        root.stopSelected(stop.row, label)
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onPressed: function (mouse) { root.selectAtY(mouse.y) }
        onPositionChanged: function (mouse) {
            if (pressed)
                root.selectAtY(mouse.y)
        }
        onReleased: root.activeLabel = ""
        onExited: {
            if (!pressed)
                root.activeLabel = ""
        }
    }

    Accessible.name: qsTr("Catalog index scrubber")
    Accessible.role: Accessible.ScrollBar
}
