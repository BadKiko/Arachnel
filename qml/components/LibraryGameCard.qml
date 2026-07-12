import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property string gameId
    required property string title
    required property string coverUrl
    required property string sourceName
    required property string version
    required property string installKindLabel
    required property bool hasUpdate
    property int componentCount: 0
    property int installedComponentCount: 0

    readonly property bool hasAddons: componentCount > 0
    readonly property string addonLabel: {
        if (!hasAddons)
            return ""
        if (installedComponentCount >= componentCount)
            return qsTr("%1 add-ons").arg(componentCount)
        if (installedComponentCount > 0)
            return qsTr("%1/%2 add-ons").arg(installedComponentCount).arg(componentCount)
        return qsTr("%1 add-ons").arg(componentCount)
    }

    property int jobRevision: 0

    readonly property var activeJob: {
        root.jobRevision
        return Core.jobs.jobForEntry(root.gameId)
    }
    readonly property bool showJobStatus: !Core.isEntryPlayable(root.gameId) && !!(activeJob.jobId)
    readonly property real posterFillProgress: {
        if (!showJobStatus)
            return -1
        if (activeJob.inProgress || activeJob.status === "installing")
            return activeJob.progress
        return 100
    }
    readonly property string statusLine: {
        if (!showJobStatus)
            return root.sourceName + " · v" + root.version
        if (activeJob.status === "installing") {
            if (activeJob.detail && activeJob.detail.length)
                return activeJob.detail
            return qsTr("Installing %1%").arg(activeJob.progress)
        }
        if (activeJob.status === "completed" && !activeJob.inProgress)
            return qsTr("Installing…")
        if (activeJob.status === "paused")
            return qsTr("Paused · %1%").arg(activeJob.progress)
        return qsTr("Downloading %1%").arg(activeJob.progress)
    }
    readonly property string statusIcon: {
        if (!showJobStatus)
            return ""
        if (activeJob.status === "installing")
            return MD.Token.icon.install_desktop
        if (activeJob.status === "paused")
            return MD.Token.icon.play_arrow
        return MD.Token.icon.pause
    }

    readonly property bool isRunning: Core.gameRunning && Core.runningGameId === root.gameId

    signal openDetails(string gameId)

    Connections {
        target: Core
        function onRunningGameChanged() { /* refresh isRunning */ }
    }

    Connections {
        target: Core.jobs
        function onJobsChanged() { root.jobRevision++ }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: MD.Token.spacing.small
        spacing: MD.Token.spacing.small

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GamePoster {
                anchors.fill: parent
                source: root.coverUrl
                seed: root.title
                fallbackText: root.title.charAt(0)
                cornerRadius: MD.Token.shape.corner.extra_large
                fillProgress: root.posterFillProgress
                onClicked: root.openDetails(root.gameId)
            }

            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: MD.Token.spacing.small
                visible: root.isRunning
                radius: MD.Token.shape.corner.full
                color: MD.Token.color.primary_container
                width: runningChipLabel.implicitWidth + 20
                height: runningChipLabel.implicitHeight + 12

                MD.Label {
                    id: runningChipLabel
                    anchors.centerIn: parent
                    text: qsTr("Playing")
                    typescale: MD.Token.typescale.label_small
                    color: MD.Token.color.on_primary_container
                }
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: MD.Token.spacing.small
                visible: root.hasUpdate
                radius: MD.Token.shape.corner.full
                color: MD.Token.color.tertiary_container
                width: updateLabel.implicitWidth + 20
                height: updateLabel.implicitHeight + 12

                MD.Label {
                    id: updateLabel
                    anchors.centerIn: parent
                    text: qsTr("Updating")
                    typescale: MD.Token.typescale.label_small
                    color: MD.Token.color.on_tertiary_container
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.margins: MD.Token.spacing.small
                visible: root.hasAddons
                radius: MD.Token.shape.corner.full
                color: MD.Token.color.secondary_container
                width: addonChipLabel.implicitWidth + 20
                height: addonChipLabel.implicitHeight + 12

                MD.Label {
                    id: addonChipLabel
                    anchors.centerIn: parent
                    text: root.addonLabel
                    typescale: MD.Token.typescale.label_small
                    color: MD.Token.color.on_secondary_container
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            text: root.title
            typescale: MD.Token.typescale.title_small
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.extra_small
            visible: root.showJobStatus || root.isRunning

            MD.Icon {
                visible: root.showJobStatus || root.isRunning
                name: root.isRunning
                      ? MD.Token.icon.sports_esports
                      : root.statusIcon
                size: 14
                color: root.isRunning
                       ? MD.Token.color.primary
                       : MD.Token.color.on_surface_variant
            }

            MD.Label {
                Layout.fillWidth: true
                text: root.isRunning ? qsTr("Running") : root.statusLine
                color: root.isRunning ? MD.Token.color.primary : MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_medium
                elide: Text.ElideRight
            }
        }

        MD.Label {
            Layout.fillWidth: true
            visible: !root.showJobStatus && !root.isRunning
            text: root.sourceName + " · v" + root.version
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_medium
            elide: Text.ElideRight
        }
    }
}
