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
    readonly property bool hasInstallFolder: {
        const lib = Core.library.gameInfo(root.gameId)
        return ((lib.installPath ?? "")).length > 0
    }
    readonly property bool showJobStatus: !root.hasInstallFolder
        && !Core.isEntryPlayable(root.gameId)
        && !!(activeJob.jobId)
        && (activeJob.inProgress
            || activeJob.status === "installing"
            || activeJob.status === "completed")
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

            MD.AssistChip {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: MD.Token.spacing.small
                visible: root.isRunning
                text: qsTr("Playing")
                elevated: true
                enabled: false
            }

            MD.AssistChip {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.margins: MD.Token.spacing.medium
                visible: root.hasUpdate
                text: qsTr("Update")
                icon.name: MD.Token.icon.update
                elevated: true
                mdState.backgroundColor: MD.Token.color.tertiary_container
                mdState.textColor: MD.Token.color.on_tertiary_container
                mdState.iconColor: MD.Token.color.on_tertiary_container
                mdState.outlineColor: MD.Token.color.tertiary_container
            }

            MD.AssistChip {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.margins: MD.Token.spacing.small
                visible: root.hasAddons
                text: root.addonLabel
                elevated: true
                enabled: false
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
