import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.ElevationRectangle {
    id: root

    property var group: ({})
    property bool expanded: false

    signal openDetails(string entryId)
    signal expansionToggled(bool expanded)

    readonly property var addons: group.addons ?? []
    readonly property bool hasAddons: !!(group.hasAddons) && addons.length > 0
    readonly property bool gameJobActive: jobIsActive(root.group)
    property real addonsPanelHeight: 0

    function updateAddonsPanelHeight() {
        const measured = addonsPanel.implicitHeight
        if (measured > 0)
            addonsPanelHeight = measured
    }

    onExpandedChanged: {
        if (expanded)
            updateAddonsPanelHeight()
        else
            Qt.callLater(updateAddonsPanelHeight)
    }

    onGroupChanged: Qt.callLater(updateAddonsPanelHeight)

    Component.onCompleted: Qt.callLater(updateAddonsPanelHeight)

    function jobIsActive(job) {
        if (!job || !job.status)
            return false
        return ["starting", "checking", "metadata", "downloading", "seeding", "paused", "installing"].includes(job.status)
    }

    function addonTitle(job) {
        const full = job.title ?? ""
        const sep = " — "
        const idx = full.indexOf(sep)
        return idx >= 0 ? full.substring(idx + sep.length) : full
    }

    function toggleExpanded() {
        root.expansionToggled(!root.expanded)
    }

    function collapsedAddonSummary() {
        let active = 0
        let done = 0
        for (let i = 0; i < addons.length; ++i) {
            if (jobIsActive(addons[i]))
                ++active
            if (addons[i].status === "completed")
                ++done
        }
        if (active > 0)
            return qsTr("%1 add-ons · %2 downloading").arg(addons.length).arg(active)
        if (done === addons.length)
            return qsTr("%1 add-ons · done").arg(addons.length)
        return qsTr("%1 add-ons").arg(addons.length)
    }

    function groupAllTerminal() {
        if (!["completed", "failed", "cancelled"].includes(group.status ?? ""))
            return false
        for (let i = 0; i < addons.length; ++i) {
            if (!["completed", "failed", "cancelled"].includes(addons[i].status ?? ""))
                return false
        }
        return true
    }

    function removeWholeGroup() {
        Core.removeJob(group.jobId)
        for (let i = 0; i < addons.length; ++i)
            Core.removeJob(addons[i].jobId)
    }

    radius: MD.Token.shape.corner.extra_large
    color: MD.Token.color.surface_container
    elevation: MD.Token.elevation.level0
    implicitHeight: groupCol.implicitHeight + 2 * MD.Token.spacing.medium

    ColumnLayout {
        id: groupCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: MD.Token.spacing.medium
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            MD.IconButton {
                visible: root.hasAddons
                mdState.type: MD.Enum.IBtStandard
                rotation: root.expanded ? 90 : 0
                icon.name: MD.Token.icon.chevron_right
                onClicked: root.toggleExpanded()

                Behavior on rotation {
                    NumberAnimation {
                        duration: MD.Token.duration.short4
                        easing: MD.Token.easing.standard
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: gameCard.implicitHeight

                DownloadJobCard {
                    id: gameCard
                    anchors.fill: parent
                    embedded: true
                    showExternalRemove: root.groupAllTerminal()
                    jobId: root.group.jobId ?? ""
                    title: root.group.title ?? ""
                    kindLabel: root.group.kindLabel ?? ""
                    status: root.group.status ?? ""
                    statusLabel: root.group.statusLabel ?? ""
                    progress: root.group.progress ?? 0
                    detail: root.group.detail ?? ""
                    coverUrl: root.group.coverUrl ?? ""
                    entryId: root.group.entryId ?? ""
                    fillProgress: root.gameJobActive ? (root.group.progress ?? 0) : -1
                    onOpenDetails: function (entryId) { root.openDetails(entryId) }
                    onRemoveRequested: root.hasAddons ? root.removeWholeGroup() : Core.removeJob(root.group.jobId)
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: root.hasAddons ? 48 : 0
            Layout.topMargin: MD.Token.spacing.extra_small
            visible: root.hasAddons && !root.expanded
            text: collapsedAddonSummary()
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_medium
            elide: Text.ElideRight
        }

        Item {
            Layout.fillWidth: true
            Layout.leftMargin: 40
            Layout.topMargin: root.expanded ? MD.Token.spacing.small : 0
            Layout.preferredHeight: root.hasAddons && root.expanded ? root.addonsPanelHeight : 0
            visible: root.hasAddons
            clip: true

            ColumnLayout {
                id: addonsPanel
                width: parent.width
                spacing: MD.Token.spacing.small

                onImplicitHeightChanged: root.updateAddonsPanelHeight()

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    MD.Icon {
                        name: MD.Token.icon.extension
                        size: 18
                        color: MD.Token.color.primary
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Add-ons")
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.on_surface_variant
                    }

                    MD.Label {
                        text: collapsedAddonSummary()
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_small
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: MD.Token.color.outline_variant
                }

                Repeater {
                    model: root.addons

                    ColumnLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 0

                        DownloadJobCard {
                            Layout.fillWidth: true
                            embedded: true
                            compact: true
                            addonRow: true
                            jobId: modelData.jobId ?? ""
                            title: root.addonTitle(modelData)
                            kindLabel: modelData.kindLabel ?? ""
                            status: modelData.status ?? ""
                            statusLabel: modelData.statusLabel ?? ""
                            progress: modelData.progress ?? 0
                            detail: modelData.detail ?? ""
                            coverUrl: modelData.coverUrl ?? ""
                            entryId: modelData.entryId ?? ""
                            parentEntryId: modelData.parentEntryId ?? root.group.entryId ?? ""
                            onOpenDetails: function (entryId) { root.openDetails(entryId) }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: MD.Token.spacing.small
                            Layout.preferredHeight: 1
                            visible: index < root.addons.length - 1
                            color: MD.Util.transparent(MD.Token.color.outline_variant, 0.65)
                        }
                    }
                }
            }
        }

    }
}
