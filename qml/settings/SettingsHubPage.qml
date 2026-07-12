import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

ColumnLayout {
    id: root

    property int contentMargin: MD.Token.spacing.large

    signal openSection(string sectionId)

    spacing: MD.Token.spacing.small

    MD.Label {
        Layout.fillWidth: true
        Layout.leftMargin: contentMargin
        Layout.rightMargin: contentMargin
        Layout.topMargin: MD.Token.spacing.small
        text: qsTr("Choose a section — each screen covers part of your setup.")
        wrapMode: Text.WordWrap
        color: MD.Token.color.on_surface_variant
        typescale: MD.Token.typescale.body_medium
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: contentMargin
        Layout.rightMargin: contentMargin
        Layout.topMargin: MD.Token.spacing.small
        spacing: MD.Token.spacing.small

        Repeater {
            model: [
                {
                    id: "plugins",
                    title: qsTr("Plugins"),
                    subtitle: qsTr("FreeTP and others — install, launch, add-ons (.arach)")
                },
                {
                    id: "sources",
                    title: qsTr("Hydra catalogs"),
                    subtitle: qsTr("games.json by URL — migrate from Hydra Launcher")
                },
                {
                    id: "storage",
                    title: qsTr("Storage"),
                    subtitle: qsTr("Library and download folders")
                },
                {
                    id: "updates",
                    title: qsTr("Updates"),
                    subtitle: qsTr("Auto-check updates and portable integrity")
                },
                {
                    id: "appearance",
                    title: qsTr("Appearance"),
                    subtitle: qsTr("Theme, palette, accent color, and language")
                }
            ]

            Rectangle {
                id: sectionCard
                required property var modelData

                Layout.fillWidth: true
                radius: MD.Token.shape.corner.large
                color: sectionMouse.containsMouse
                       ? MD.Token.color.surface_container_high
                       : MD.Token.color.surface_container
                border.width: 1
                border.color: MD.Token.color.outline_variant
                implicitHeight: sectionRow.implicitHeight + MD.Token.spacing.medium * 2

                Behavior on color {
                    ColorAnimation { duration: MD.Token.duration.short4 }
                }

                RowLayout {
                    id: sectionRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: MD.Token.spacing.medium
                    anchors.rightMargin: MD.Token.spacing.medium
                    spacing: MD.Token.spacing.medium

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        MD.Label {
                            Layout.fillWidth: true
                            text: sectionCard.modelData.title
                            typescale: MD.Token.typescale.title_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: sectionCard.modelData.subtitle
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_small
                            elide: Text.ElideRight
                        }
                    }

                    MD.Icon {
                        name: MD.Token.icon.chevron_right
                        size: 24
                        color: MD.Token.color.on_surface_variant
                    }
                }

                MouseArea {
                    id: sectionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.openSection(sectionCard.modelData.id)
                }
            }
        }
    }
}
