import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int pageMargin: MD.Token.spacing.large
    property var jobGroups: []
    property var expandedGroups: ({})
    readonly property bool downloadsEmpty: jobGroups.length === 0

    function refreshGroups() {
        refreshDebounce.restart()
    }

    Timer {
        id: refreshDebounce
        interval: 120
        onTriggered: jobGroups = Core.jobs.downloadGroups()
    }

    function isGroupExpanded(entryId) {
        return !!(entryId && expandedGroups[entryId])
    }

    function setGroupExpanded(entryId, value) {
        if (!entryId)
            return
        const next = Object.assign({}, expandedGroups)
        if (value)
            next[entryId] = true
        else
            delete next[entryId]
        expandedGroups = next
    }

    function countFinished() {
        return Core.jobs.count - Core.jobs.activeCount
    }

    Connections {
        target: Core.jobs
        function onJobsChanged() { root.refreshGroups() }
        function onCountChanged() { root.refreshGroups() }
    }

    Component.onCompleted: refreshGroups()

    signal openGame(string gameId)

    // ── Empty (как «Нет игр» в каталоге) ─────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: root.downloadsEmpty

        ColumnLayout {
            anchors.centerIn: parent
            spacing: MD.Token.spacing.medium
            width: Math.min(parent.width - pageMargin * 2, 420)

            SpiderWebMark {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 160
                Layout.preferredHeight: 160
                width: 160
                height: 160
                strokeColor: MD.Token.color.primary
                strokeWidth: 2.5
                opacity: 0.35
            }

            MD.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No downloads")
                typescale: MD.Token.typescale.title_large
            }

            MD.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: Messages.downloadsEmptyHint
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WordWrap
            }
        }
    }

    // ── Список загрузок ────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: MD.Token.spacing.medium
        visible: !root.downloadsEmpty

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
            Layout.topMargin: MD.Token.spacing.medium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    text: qsTr("Downloads")
                    typescale: MD.Token.typescale.headline_medium
                }

                MD.Label {
                    text: Core.jobs.activeCount > 0
                          ? qsTr("%1 active · resume after restart").arg(Core.jobs.activeCount)
                          : qsTr("Torrents resume after restart")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }
            }

            MD.IconButton {
                visible: root.countFinished() > 0
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.delete_sweep
                onClicked: Core.clearFinishedJobs()
            }
        }

        ListView {
            id: jobsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
            Layout.bottomMargin: pageMargin
            clip: true
            spacing: MD.Token.spacing.small
            boundsBehavior: Flickable.StopAtBounds
            model: root.jobGroups

            ScrollBar.vertical: MD.ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: DownloadJobGroupCard {
                width: jobsList.width
                group: modelData
                expanded: root.isGroupExpanded(modelData.entryId)
                onExpansionToggled: function (value) {
                    root.setGroupExpanded(modelData.entryId, value)
                }
                onOpenDetails: function (entryId) { root.openGame(entryId) }
            }
        }
    }
}
