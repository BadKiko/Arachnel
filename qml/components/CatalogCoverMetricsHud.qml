import QtQuick

import Arachnel.Core 1.0
import Qcm.Material as MD

// Live cover-pipeline counters for scroll / load testing.
// Left-click: reset. Right-click: hide. Ctrl+Shift+M toggles (see CatalogPage).
Rectangle {
    id: root

    property bool expanded: true
    property string line: "cover …"

    width: Math.min(label.implicitWidth + 16, parent ? parent.width - 24 : 640)
    height: label.implicitHeight + 10
    radius: 6
    color: Qt.rgba(0, 0, 0, 0.72)
    border.color: Qt.rgba(1, 1, 1, 0.12)
    border.width: 1
    visible: root.expanded
    z: 1000

    Timer {
        interval: 400
        running: root.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: root.line = Core.coverMetricsText()
    }

    MD.Label {
        id: label
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        text: root.line
        color: "#E8E8E8"
        typescale: MD.Token.typescale.label_small
        wrapMode: Text.WordWrap
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                root.expanded = false
                return
            }
            Core.resetCoverFetchMetrics()
            root.line = Core.coverMetricsText()
        }
    }
}
