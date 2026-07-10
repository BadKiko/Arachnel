import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

MD.Pane {
    id: root

    property int currentIndex: 0
    property var model: []
    property int downloadBadge: 0

    signal activated(int index)
    signal settingsRequested()

    padding: 0
    backgroundColor: MD.Token.color.surface_container
    // Wide enough for "Библиотека" under icon
    implicitWidth: 108

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: MD.Token.spacing.medium
        anchors.bottomMargin: MD.Token.spacing.medium
        spacing: MD.Token.spacing.extra_small

        MD.ElevationRectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            radius: MD.Token.shape.corner.extra_large
            color: MD.Token.color.primary_container
            elevation: MD.Token.elevation.level0

            SpiderWebMark {
                anchors.centerIn: parent
                width: 28
                height: 28
                strokeColor: MD.Token.color.on_primary_container
                strokeWidth: 1.6
                rings: 3
                spokes: 8
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.currentIndex = 0
                    root.activated(0)
                }
            }
        }

        Item { Layout.preferredHeight: MD.Token.spacing.small }

        Repeater {
            model: root.model

            Item {
                id: railEntry
                required property int index
                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: 72
                Layout.leftMargin: MD.Token.spacing.small
                Layout.rightMargin: MD.Token.spacing.small

                readonly property bool selected: index === root.currentIndex

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4

                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 64
                        Layout.preferredHeight: 36

                        MD.ElevationRectangle {
                            anchors.centerIn: parent
                            width: 64
                            height: 36
                            radius: MD.Token.shape.corner.full
                            color: railEntry.selected
                                   ? MD.Token.color.secondary_container
                                   : "transparent"
                            elevation: MD.Token.elevation.level0

                            Behavior on color {
                                ColorAnimation { duration: MD.Token.duration.short4 }
                            }

                            MD.Icon {
                                anchors.centerIn: parent
                                name: railEntry.modelData.icon
                                size: 24
                                color: railEntry.selected
                                       ? MD.Token.color.on_secondary_container
                                       : MD.Token.color.on_surface_variant
                            }

                            MD.Badge {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.rightMargin: 4
                                anchors.topMargin: 2
                                visible: railEntry.modelData.navIndex === 2 && root.downloadBadge > 0
                                text: root.downloadBadge > 9 ? "9+" : String(root.downloadBadge)
                            }
                        }
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 2
                        Layout.rightMargin: 2
                        horizontalAlignment: Text.AlignHCenter
                        text: railEntry.modelData.name
                        typescale: MD.Token.typescale.label_small
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        color: railEntry.selected
                               ? MD.Token.color.on_surface
                               : MD.Token.color.on_surface_variant
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.currentIndex = railEntry.index
                        root.activated(railEntry.index)
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        MD.IconButton {
            Layout.alignment: Qt.AlignHCenter
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.settings
            onClicked: root.settingsRequested()
        }
    }
}
