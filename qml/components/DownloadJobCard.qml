import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.ElevationRectangle {
    id: root

    property string jobId: ""
    property string title: ""
    property string kindLabel: ""
    property string status: ""
    property string statusLabel: ""
    property int progress: 0
    property string detail: ""
    property string coverUrl: ""
    property string entryId: ""

    readonly property bool inProgress: ["starting", "checking", "metadata", "downloading", "seeding", "paused"].includes(status)
    readonly property bool isPaused: status === "paused"
    readonly property bool isFailed: status === "failed" || status === "cancelled"
    readonly property bool isTerminal: status === "completed" || status === "failed" || status === "cancelled"
    readonly property bool canRetry: status === "failed" || status === "cancelled"

    radius: MD.Token.shape.corner.extra_large
    color: MD.Token.color.surface_container
    elevation: MD.Token.elevation.level0
    implicitHeight: row.implicitHeight + 2 * MD.Token.spacing.medium

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: MD.Token.spacing.medium
        spacing: MD.Token.spacing.medium

        GamePoster {
            Layout.preferredWidth: 88
            Layout.preferredHeight: 118
            Layout.alignment: Qt.AlignTop
            source: root.coverUrl
            seed: root.title
            fallbackText: root.title.charAt(0)
            cornerRadius: MD.Token.shape.corner.large
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: MD.Token.spacing.small

            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.small

                MD.Label {
                    Layout.fillWidth: true
                    text: root.title
                    typescale: MD.Token.typescale.title_small
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                MD.Label {
                    visible: root.inProgress || status === "completed"
                    text: root.progress + "%"
                    typescale: MD.Token.typescale.label_large
                    color: MD.Token.color.primary
                }

                MD.IconButton {
                    visible: root.inProgress
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: root.isPaused ? MD.Token.icon.play_arrow : MD.Token.icon.pause
                    onClicked: Core.toggleJobPause(root.jobId)
                }

                MD.IconButton {
                    visible: root.inProgress
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.close
                    onClicked: Core.cancelJob(root.jobId)
                }

                MD.IconButton {
                    visible: root.canRetry
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.refresh
                    onClicked: Core.retryJob(root.jobId)
                }

                MD.IconButton {
                    visible: root.isTerminal
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.delete
                    onClicked: Core.removeJob(root.jobId)
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                clip: true

                Rectangle {
                    anchors.fill: parent
                    radius: 2
                    color: MD.Util.transparent(MD.Token.color.primary, 0.25)
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: parent.width * (root.progress / 100)
                    radius: 2
                    color: root.isPaused ? MD.Token.color.on_surface_variant : MD.Token.color.primary
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.small

                MD.Label {
                    Layout.fillWidth: true
                    text: root.detail.length ? root.detail : root.statusLabel
                    color: root.isFailed ? MD.Token.color.error : MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_medium
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                MD.Label {
                    visible: root.statusLabel.length > 0 && root.detail.length > 0
                    text: root.statusLabel
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_small
                }
            }
        }
    }
}
