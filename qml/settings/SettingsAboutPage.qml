import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large

    readonly property string platformLabel: {
        switch (Qt.platform.os) {
        case "windows":
            return qsTr("Windows")
        case "linux":
            return qsTr("Linux")
        case "osx":
            return qsTr("macOS")
        default:
            return Qt.platform.os
        }
    }

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: qsTr("Game launcher with plugin-based sources and Hydra catalogs.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: aboutColumn.implicitHeight + MD.Token.spacing.medium * 2

            ColumnLayout {
                id: aboutColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Application")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_large
                    }

                    MD.Label {
                        text: Qt.application.name.length ? Qt.application.name : "Arachnel"
                        typescale: MD.Token.typescale.body_large
                        horizontalAlignment: Text.AlignRight
                    }
                }

                MD.Divider { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Version")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_large
                    }

                    MD.Label {
                        text: Qt.application.version.length ? ("v" + Qt.application.version) : qsTr("Unknown")
                        typescale: MD.Token.typescale.body_large
                        horizontalAlignment: Text.AlignRight
                    }
                }

                MD.Divider { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Platform")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_large
                    }

                    MD.Label {
                        text: root.platformLabel
                        typescale: MD.Token.typescale.body_large
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }
    }
}
