import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

Item {
    id: root

    property string title: ""
    property string value: ""
    property string iconName: ""

    implicitHeight: 88
    Layout.fillWidth: true
    Layout.minimumHeight: 88

    MD.ElevationRectangle {
        anchors.fill: parent
        radius: MD.Token.shape.corner.extra_large
        color: MD.Token.color.surface_container
        elevation: MD.Token.elevation.level1

        RowLayout {
            anchors.fill: parent
            anchors.margins: MD.Token.spacing.medium
            spacing: MD.Token.spacing.medium

            MD.ElevationRectangle {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                Layout.alignment: Qt.AlignVCenter
                radius: MD.Token.shape.corner.full
                color: MD.Token.color.primary_container
                elevation: MD.Token.elevation.level0

                MD.Icon {
                    anchors.centerIn: parent
                    name: root.iconName
                    size: 24
                    color: MD.Token.color.on_primary_container
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                MD.Label {
                    Layout.fillWidth: true
                    text: root.value
                    typescale: MD.Token.typescale.headline_small
                    elide: Text.ElideRight
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: root.title
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_large
                    elide: Text.ElideRight
                }
            }
        }
    }
}
