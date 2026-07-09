import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.ElevationRectangle {
    id: root

    required property string jobId
    required property string title
    required property string kindLabel
    required property string status
    required property int progress
    required property string detail

    radius: MD.Token.shape.corner.large
    color: MD.Token.color.surface_container_high
    elevation: MD.Token.elevation.level0
    implicitHeight: content.implicitHeight + 2 * MD.Token.spacing.medium

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: MD.Token.spacing.medium
        spacing: MD.Token.spacing.small

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                text: root.title
                typescale: MD.Token.typescale.title_small
                elide: Text.ElideRight
            }

            MD.AssistChip {
                text: root.kindLabel
                icon.name: MD.Token.icon.download
            }

            MD.IconButton {
                visible: root.status !== "completed" && root.status !== "failed" && root.status !== "cancelled"
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.close
                onClicked: Core.cancelJob(root.jobId)
            }
        }

        MD.LinearIndicator {
            Layout.fillWidth: true
            value: root.progress / 100
        }

        RowLayout {
            Layout.fillWidth: true

            MD.Label {
                Layout.fillWidth: true
                text: root.detail
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_medium
                elide: Text.ElideRight
            }

            MD.Label {
                text: root.status
                color: MD.Token.color.primary
                typescale: MD.Token.typescale.label_medium
            }
        }
    }
}
