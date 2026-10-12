import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// Centered placeholder for the first catalog load, when the list is still empty: a slowly turning
// expressive shape (still under "reduce motion") with the reason for the wait. Without it the page
// is an empty slab for as long as the feed takes.
Item {
    id: root

    ColumnLayout {
        anchors.centerIn: parent
        spacing: MD.Token.spacing.medium
        width: Math.min(root.width - 48, 420)

        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 112
            Layout.preferredHeight: 112

            ExpressiveShape {
                id: shape
                anchors.fill: parent
                kind: "cookie12"
                color: MD.Token.color.primary_container

                RotationAnimator on rotation {
                    from: 0
                    to: 360
                    duration: 9000
                    loops: Animation.Infinite
                    running: !AppMotion.reduceMotion && root.visible
                }
            }

            MD.Icon {
                anchors.centerIn: parent
                name: MD.Token.icon.download
                size: 40
                color: MD.Token.color.on_primary_container
                opacity: pulse.running ? 1 : 0.9

                SequentialAnimation on opacity {
                    id: pulse
                    loops: Animation.Infinite
                    running: !AppMotion.reduceMotion && root.visible
                    NumberAnimation { from: 1; to: 0.45; duration: 900; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.45; to: 1; duration: 900; easing.type: Easing.InOutSine }
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.topMargin: MD.Token.spacing.small
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Loading the catalog")
            typescale: MD.Token.typescale.headline_small
            font.weight: Font.DemiBold
        }

        MD.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Getting the list of games ready. The first start takes longer; after that the catalog opens from the saved copy.")
            typescale: MD.Token.typescale.body_medium
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
        }
    }
}
