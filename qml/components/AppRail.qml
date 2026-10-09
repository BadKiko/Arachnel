import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

MD.Pane {
    id: root

    property int currentIndex: 0
    property var model: []
    property int downloadBadge: 0
    // Icons only: set by the window below ~1000 px so the content keeps its room.
    property bool compact: false

    signal activated(int index)
    signal settingsRequested()

    padding: 0
    backgroundColor: MD.Token.color.surface_container
    // Wide enough for "Библиотека" under the icon; icons-only when compact.
    implicitWidth: compact ? 84 : 108
    Behavior on implicitWidth {
        NumberAnimation {
            duration: AppMotion.medium
            easing: AppMotion.emphasizedDecelerate
        }
    }

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
                onClicked: root.activated(0)
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
                Layout.preferredHeight: root.compact ? 56 : 76
                Layout.leftMargin: MD.Token.spacing.small
                Layout.rightMargin: MD.Token.spacing.small

                readonly property bool selected: index === root.currentIndex
                readonly property bool hovered: entryArea.containsMouse

                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: modelData.name
                Keys.onReturnPressed: root.activated(railEntry.index)
                Keys.onEnterPressed: root.activated(railEntry.index)
                Keys.onSpacePressed: root.activated(railEntry.index)

                Behavior on Layout.preferredHeight {
                    NumberAnimation {
                        duration: AppMotion.medium
                        easing: AppMotion.emphasizedDecelerate
                    }
                }

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
                                   : railEntry.hovered
                                     ? MD.Util.transparent(MD.Token.color.on_surface, 0.08)
                                     : "transparent"
                            elevation: MD.Token.elevation.level0
                            scale: railEntry.selected ? 1 : 0.92
                            transformOrigin: Item.Center

                            Behavior on color {
                                ColorAnimation {
                                    duration: AppMotion.short
                                    easing.type: Easing.Linear
                                }
                            }
                            Behavior on scale {
                                NumberAnimation {
                                    duration: AppMotion.short
                                    easing: AppMotion.emphasizedDecelerate
                                }
                            }

                            MD.Icon {
                                anchors.centerIn: parent
                                name: railEntry.modelData.icon
                                size: 24
                                color: railEntry.selected
                                       ? MD.Token.color.on_secondary_container
                                       : MD.Token.color.on_surface_variant

                                Behavior on color {
                                    ColorAnimation { duration: AppMotion.short }
                                }
                            }

                            MD.Badge {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.rightMargin: 4
                                anchors.topMargin: 2
                                visible: !!railEntry.modelData.showDownloadBadge && root.downloadBadge > 0
                                text: root.downloadBadge > 9 ? "9+" : String(root.downloadBadge)
                                backgroundColor: MD.Token.color.primary
                                textColor: MD.Token.color.on_primary
                            }
                        }

                        // Keyboard focus ring (Tab traversal).
                        Rectangle {
                            anchors.centerIn: parent
                            width: 64
                            height: 36
                            radius: height / 2
                            color: "transparent"
                            border.width: 2
                            border.color: MD.Token.color.primary
                            visible: railEntry.activeFocus
                        }
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 2
                        Layout.rightMargin: 2
                        horizontalAlignment: Text.AlignHCenter
                        text: railEntry.modelData.name
                        typescale: MD.Token.typescale.label_medium
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        opacity: root.compact ? 0 : 1
                        visible: opacity > 0
                        color: railEntry.selected
                               ? MD.Token.color.on_surface
                               : MD.Token.color.on_surface_variant

                        Behavior on opacity {
                            NumberAnimation {
                                duration: AppMotion.short
                                easing: AppMotion.standard
                            }
                        }
                    }
                }

                MouseArea {
                    id: entryArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    // Don't assign currentIndex here - that breaks the parent's
                    // `currentIndex: pageIndex` binding and leaves the rail stuck
                    // (e.g. Discover stays highlighted after "All games" → Catalog).
                    onClicked: root.activated(railEntry.index)

                    MD.ToolTip {
                        visible: root.compact && entryArea.containsMouse
                        text: railEntry.modelData.name
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
