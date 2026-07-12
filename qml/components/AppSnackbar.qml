import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

Item {
    id: root

    property string message: ""
    property bool open: false
    readonly property real railInset: 88

    function show(text, durationMs) {
        if (!text || text.length === 0)
            return
        message = text
        open = true
        dismissTimer.interval = durationMs ?? 5000
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

    MD.ElevationRectangle {
        id: bar
        z: 1
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: MD.Token.spacing.large
        width: Math.min(
                   contentRow.implicitWidth + MD.Token.spacing.large * 2,
                   parent.width - root.railInset - MD.Token.spacing.large * 2)
        implicitHeight: Math.max(48, contentRow.implicitHeight + MD.Token.spacing.medium * 2)
        radius: MD.Token.shape.corner.extra_large
        color: MD.Token.color.surface_container_high
        elevation: MD.Token.elevation.level2
        border.width: 1
        border.color: MD.Token.color.outline_variant
        opacity: root.open ? 1 : 0
        scale: root.open ? 1 : 0.94
        visible: opacity > 0.01 || root.open

        Behavior on opacity {
            NumberAnimation {
                duration: MD.Token.duration.short4
                easing: MD.Token.easing.standard
            }
        }

        Behavior on scale {
            NumberAnimation {
                duration: MD.Token.duration.medium2
                easing: MD.Token.easing.emphasized_decelerate
            }
        }

        RowLayout {
            id: contentRow
            anchors.centerIn: parent
            width: parent.width - MD.Token.spacing.large * 2
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                text: root.message
                color: MD.Token.color.on_surface
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WordWrap
                maximumLineCount: 2
            }

            MD.IconButton {
                Layout.alignment: Qt.AlignVCenter
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.close
                icon.color: MD.Token.color.on_surface_variant
                onClicked: root.dismiss()
            }
        }
    }
}
