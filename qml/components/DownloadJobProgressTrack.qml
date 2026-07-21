import QtQuick
import QtQuick.Layouts
import Qcm.Material as MD

Item {
    required property var page
    implicitHeight: 4
                Item {
                    id: progressTrack
                    Layout.fillWidth: true
                    Layout.preferredHeight: page.addonRow ? 3 : 4
                    clip: true

                    Rectangle {
                        anchors.fill: parent
                        radius: 2
                        color: MD.Util.transparent(MD.Token.color.primary, 0.25)
                    }

                    Rectangle {
                        id: installIndeterminate
                        visible: page.isInstalling
                        height: parent.height
                        width: Math.max(24, parent.width * 0.35)
                        radius: 2
                        color: MD.Token.color.primary
                        x: -width

                        SequentialAnimation {
                            running: page.isInstalling
                            loops: Animation.Infinite
                            NumberAnimation {
                                target: installIndeterminate
                                property: "x"
                                from: -installIndeterminate.width
                                to: progressTrack.width
                                duration: 900
                                easing.type: Easing.InOutQuad
                            }
                            NumberAnimation {
                                target: installIndeterminate
                                property: "x"
                                from: progressTrack.width
                                to: -installIndeterminate.width
                                duration: 900
                                easing.type: Easing.InOutQuad
                            }
                        }
                    }

                    Rectangle {
                        visible: !page.isInstalling
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        width: parent.width * (page.progress / 100)
                        radius: 2
                        color: page.isPaused ? MD.Token.color.on_surface_variant : MD.Token.color.primary
                    }
                }
}
