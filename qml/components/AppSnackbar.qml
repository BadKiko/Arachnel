import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

Item {
    id: root

    property string message: ""
    property bool open: false

    function show(text, durationMs) {
        if (!text || text.length === 0)
            return
        message = text
        open = true
        dismissTimer.interval = durationMs ?? 4500
        dismissTimer.restart()
    }

    function dismiss() {
        open = false
        dismissTimer.stop()
    }

    Timer {
        id: dismissTimer
        onTriggered: root.dismiss()
    }

    Item {
        id: barHost
        z: 1
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: MD.Token.spacing.large
        width: Math.min(bar.implicitWidth, parent.width - railInset - MD.Token.spacing.large * 2)
        height: bar.implicitHeight
        opacity: root.open ? 1 : 0
        y: root.open ? 0 : 20

        readonly property real railInset: 88

        Behavior on opacity {
            NumberAnimation {
                duration: MD.Token.duration.short4
                easing: MD.Token.easing.standard
            }
        }

        Behavior on y {
            NumberAnimation {
                duration: MD.Token.duration.medium4
                easing: MD.Token.easing.emphasized_decelerate
            }
        }

        MD.ElevationRectangle {
            id: bar
            anchors.fill: parent
            radius: MD.Token.shape.corner.small
            color: MD.Token.color.inverse_surface
            elevation: MD.Token.elevation.level3

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: MD.Token.spacing.medium
                anchors.rightMargin: MD.Token.spacing.extra_small
                anchors.topMargin: MD.Token.spacing.small
                anchors.bottomMargin: MD.Token.spacing.small
                spacing: MD.Token.spacing.small

                MD.Label {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 480
                    text: root.message
                    color: MD.Token.color.inverse_on_surface
                    typescale: MD.Token.typescale.body_medium
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }

                MD.IconButton {
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.close
                    icon.color: MD.Token.color.inverse_on_surface
                    onClicked: root.dismiss()
                }
            }
        }
    }
}
