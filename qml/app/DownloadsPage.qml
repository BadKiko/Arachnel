import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int pageMargin: MD.Token.spacing.large
    property var expandedGroups: ({})
    readonly property bool downloadsEmpty: groupsModel.count === 0

    ListModel {
        id: groupsModel
    }

    function refreshGroups() {
        refreshDebounce.restart()
    }

    function applyGroups(nextGroups) {
        const next = nextGroups || []
        const list = jobsList

        let structural = groupsModel.count !== next.length
        if (!structural) {
            for (let i = 0; i < next.length; ++i) {
                if ((groupsModel.get(i).jobId ?? "") !== (next[i].jobId ?? "")) {
                    structural = true
                    break
                }
            }
        }

        if (structural) {
            const restore = list.visible && list.count > 0
                    && !list.moving && !list.flicking && !list.dragging
            const y = restore ? list.contentY : 0
            groupsModel.clear()
            for (let i = 0; i < next.length; ++i)
                groupsModel.append({ group: next[i], jobId: next[i].jobId ?? "" })
            if (!restore)
                return
            function restoreY() {
                if (!list || list.moving || list.flicking || list.dragging)
                    return
                const maxY = Math.max(0, list.contentHeight - list.height)
                list.contentY = Math.min(Math.max(0, y), maxY)
            }
            Qt.callLater(function () {
                restoreY()
                Qt.callLater(restoreY)
            })
            return
        }

        // Same job rows — update in place so progress bars don't remount / re-animate.
        for (let i = 0; i < next.length; ++i) {
            groupsModel.setProperty(i, "group", next[i])
            groupsModel.setProperty(i, "jobId", next[i].jobId ?? "")
        }
    }

    Timer {
        id: refreshDebounce
        interval: 120
        onTriggered: root.applyGroups(Core.jobs.downloadGroups())
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
            spacing: MD.Token.spacing.medium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    text: qsTr("Downloads")
                    typescale: MD.Token.typescale.headline_medium
                }

                MD.Label {
                    text: {
                        const active = Core.jobs.activeCount
                        const finished = root.countFinished()
                        if (active > 0 && finished > 0)
                            return qsTr("%1 active · %2 finished · resume after restart")
                                    .arg(active).arg(finished)
                        if (active > 0)
                            return qsTr("%1 active · resume after restart").arg(active)
                        if (finished > 0)
                            return qsTr("%1 finished · torrents resume after restart").arg(finished)
                        return qsTr("Torrents resume after restart")
                    }
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                    elide: Text.ElideRight
                }
            }

            MD.Button {
                visible: root.countFinished() > 0
                text: qsTr("Clear finished")
                icon.name: MD.Token.icon.delete_sweep
                mdState.type: MD.Enum.BtText
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
            spacing: MD.Token.spacing.medium
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            cacheBuffer: height * 2
            model: groupsModel

            ScrollBar.vertical: MD.ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Item {
                id: rowRoot
                width: jobsList.width
                height: Math.ceil(card.implicitHeight)
                required property var group
                required property string jobId

                DownloadJobGroupCard {
                    id: card
                    width: parent.width
                    group: rowRoot.group
                    expanded: root.isGroupExpanded(rowRoot.group.entryId ?? "")
                    onExpansionToggled: function (value) {
                        root.setGroupExpanded(rowRoot.group.entryId ?? "", value)
                    }
                    onOpenDetails: function (entryId) { root.openGame(entryId) }
                }
            }
        }
    }
}
