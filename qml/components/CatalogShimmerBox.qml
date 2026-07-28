import QtQuick

import Qcm.Material as MD

Item {
    id: root

    property int cornerRadius: MD.Token.shape.corner.large
    property bool running: true

    implicitWidth: 160
    implicitHeight: 100

    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        color: MD.Token.color.surface_container_high
        clip: true

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: MD.Util.transparent(MD.Token.color.primary, 0.05)
                }
                GradientStop {
                    position: 1
                    color: MD.Util.transparent(MD.Token.color.primary, 0.01)
                }
            }
        }

        Item {
            id: shimmerClip
            anchors.fill: parent
            clip: true

            Rectangle {
                id: shimmerBand
                width: parent.width * 0.42
                height: parent.height * 1.5
                rotation: 16
                anchors.verticalCenter: parent.verticalCenter
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop {
                        position: 0.5
                        color: MD.Util.transparent(MD.Token.color.on_surface, 0.12)
                    }
                    GradientStop { position: 1.0; color: "transparent" }
                }

                SequentialAnimation on x {
                    running: root.running && root.visible
                    loops: Animation.Infinite
                    NumberAnimation {
                        from: -shimmerBand.width
                        to: shimmerClip.width + shimmerBand.width
                        duration: 1500
                        easing.type: Easing.InOutCubic
                    }
                    PauseAnimation { duration: 320 }
                }
            }
        }
    }
}
