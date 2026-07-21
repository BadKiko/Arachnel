import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    property string jobId: ""
    property string title: ""
    property string kindLabel: ""
    property string status: ""
    property string statusLabel: ""
    property int progress: 0
    property real bytesDownloaded: 0
    property real totalBytes: 0
    property string detail: ""
    property string coverUrl: ""
    property string entryId: ""
    property string parentEntryId: ""
    property bool embedded: false
    property bool compact: false
    property bool addonRow: false
    property bool showExternalRemove: false
    property real fillProgress: -1
    property real catalogTotalBytes: 0

    signal removeRequested()
    signal openDetails(string entryId)

    readonly property string detailsEntryId: root.parentEntryId.length
                                            ? root.parentEntryId
                                            : root.entryId

    readonly property bool inProgress: ["starting", "checking", "metadata", "downloading", "seeding", "paused", "installing"].includes(status)
    readonly property bool isInstalling: status === "installing"
    readonly property bool isPaused: status === "paused"
    readonly property bool isFailed: status === "failed" || status === "cancelled"
    readonly property bool isCompleted: status === "completed" && !root.installFailed
    readonly property bool isTerminal: status === "completed" || status === "failed" || status === "cancelled"
    readonly property bool canRetry: status === "failed" || status === "cancelled"
    readonly property bool canRetryInstall: root.jobId.length > 0 && Core.canRetryJobInstall(root.jobId)
    readonly property bool canManualInstall: root.jobId.length > 0 && Core.canManualInstallJob(root.jobId)
    readonly property bool installFailed: root.detail.indexOf("Install failed") >= 0

    property real _lastBytesSample: 0
    property real _lastBytesAtMs: 0
    property real estimatedRateBps: 0

    readonly property real effectiveTotalBytes: {
        const jobTotal = root.totalBytes || 0
        if (jobTotal > 0)
            return jobTotal
        return root.catalogTotalBytes || 0
    }

    readonly property real effectiveDownloaded: {
        const raw = root.bytesDownloaded || 0
        const total = root.effectiveTotalBytes
        if (raw > 0)
            return raw
        if (total > 0 && root.progress > 0)
            return total * root.progress / 100
        return 0
    }

    readonly property string displayTitle: {
        let t = (root.title || "").trim()
        const prefixes = [
            "Downloading ", "Загрузка ",
            "Installing ", "Установка ",
            "Updating ", "Обновление "
        ]
        for (let i = 0; i < prefixes.length; ++i) {
            if (t.startsWith(prefixes[i])) {
                t = t.substring(prefixes[i].length).trim()
                break
            }
        }
        return t.length ? t : qsTr("Unknown download")
    }

    readonly property color statusAccent: {
        if (root.installFailed || root.isFailed)
            return MD.Token.color.error
        if (root.isCompleted)
            return MD.Token.color.tertiary
        if (root.isPaused)
            return MD.Token.color.on_surface_variant
        if (root.inProgress)
            return MD.Token.color.primary
        return MD.Token.color.on_surface_variant
    }

    readonly property string statusChipText: {
        if (root.installFailed)
            return qsTr("Install failed")
        if (root.statusLabel.length)
            return root.statusLabel
        return root.status
    }

    function formatByteCount(n) {
        if (!n || n <= 0)
            return "0 B"
        const units = ["B", "KB", "MB", "GB", "TB"]
        let v = n
        let u = 0
        while (v >= 1024 && u < units.length - 1) {
            v /= 1024
            u++
        }
        return (u === 0 ? Math.round(v) : v.toFixed(1)) + " " + units[u]
    }

    function formatSpeed(bps) {
        if (!bps || bps < 8 * 1024)
            return ""
        return formatByteCount(bps) + "/s"
    }

    function formatEta(remaining, bps) {
        if (!bps || bps <= 0 || !remaining || remaining <= 0)
            return ""
        const seconds = Math.floor(remaining / bps)
        if (seconds < 60)
            return seconds + " s"
        if (seconds < 3600)
            return Math.floor(seconds / 60) + " min"
        return Math.floor(seconds / 3600) + " h"
    }

    function isGenericStatusDetail(d) {
        if (!d || d.length === 0)
            return true
        return d === "Downloading…"
                || d === "Загрузка…"
                || d === "Installed"
                || d === "Установлено"
                || d === "Установлен"
                || d.indexOf("Preparing") >= 0
                || d.indexOf("Подготовка") >= 0
                || d.indexOf("Getting game info") >= 0
    }

    readonly property string sizeLine: {
        const downloaded = root.effectiveDownloaded
        const total = root.effectiveTotalBytes
        if (total > 0)
            return formatByteCount(downloaded) + " / " + formatByteCount(total)
        if (downloaded > 0)
            return formatByteCount(downloaded)
        return ""
    }

    readonly property string transferMeta: {
        if (root.isInstalling || root.isCompleted || root.isFailed)
            return ""
        const d = (root.detail || "").trim()
        if (d.length > 0 && !isGenericStatusDetail(d))
            return d

        const speed = formatSpeed(root.estimatedRateBps)
        const eta = formatEta(Math.max(0, root.effectiveTotalBytes - root.effectiveDownloaded),
                              root.estimatedRateBps)
        const parts = []
        if (speed.length)
            parts.push(speed)
        if (eta.length)
            parts.push("ETA " + eta)
        return parts.join(" · ")
    }

    readonly property string secondaryLine: {
        const d = (root.detail || "").trim()
        if (root.installFailed || (root.isFailed && d.length && !isGenericStatusDetail(d)))
            return d
        if (root.isInstalling && d.length && !isGenericStatusDetail(d))
            return d

        const size = root.sizeLine
        const meta = root.transferMeta
        if (size.length && meta.length)
            return size + " · " + meta
        if (size.length)
            return size
        if (meta.length)
            return meta
        return ""
    }

    onBytesDownloadedChanged: {
        const now = Date.now()
        const sample = root.effectiveDownloaded
        if (_lastBytesAtMs > 0 && sample > _lastBytesSample) {
            const dt = (now - _lastBytesAtMs) / 1000
            const delta = sample - _lastBytesSample
            if (dt > 0.4 && delta > 0)
                root.estimatedRateBps = delta / dt
        }
        _lastBytesSample = sample
        _lastBytesAtMs = now
    }

    onProgressChanged: {
        if ((root.bytesDownloaded || 0) <= 0)
            _lastBytesSample = root.effectiveDownloaded
    }

    Timer {
        interval: 250
        running: root.inProgress && !root.isPaused && !root.isInstalling
        repeat: true
        onTriggered: {
            const sample = root.effectiveDownloaded
            const now = Date.now()
            if (_lastBytesAtMs > 0 && sample > _lastBytesSample) {
                const dt = (now - _lastBytesAtMs) / 1000
                const delta = sample - _lastBytesSample
                if (dt > 0.1 && delta > 0 && delta < 80 * 1024 * 1024)
                    root.estimatedRateBps = delta / dt
            }
            _lastBytesSample = sample
            _lastBytesAtMs = now
        }
    }

    implicitWidth: embedded ? (parent ? parent.width : implicitWidth) : implicitWidth
    implicitHeight: content.implicitHeight + (embedded ? 0 : 2 * MD.Token.spacing.large)

    MD.ElevationRectangle {
        anchors.fill: parent
        visible: !root.embedded
        radius: MD.Token.shape.corner.extra_large
        color: MD.Token.color.surface_container
        elevation: MD.Token.elevation.level0
    }

    Item {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: root.embedded
                         ? (root.compact ? MD.Token.spacing.small : 0)
                         : MD.Token.spacing.large
        implicitHeight: row.implicitHeight

        RowLayout {
            id: row
            width: parent.width
            spacing: root.addonRow ? MD.Token.spacing.small : MD.Token.spacing.medium

            MD.Icon {
                visible: root.addonRow
                Layout.alignment: Qt.AlignVCenter
                name: MD.Token.icon.extension
                size: 18
                color: root.statusAccent
            }

            Item {
                visible: !root.compact
                Layout.preferredWidth: 96
                Layout.preferredHeight: 128
                Layout.alignment: Qt.AlignTop

                GamePoster {
                    anchors.fill: parent
                    source: root.coverUrl
                    seed: root.displayTitle
                    fallbackText: root.displayTitle.charAt(0)
                    cornerRadius: MD.Token.shape.corner.large
                    fillProgress: root.fillProgress
                    onClicked: {
                        if (root.detailsEntryId.length)
                            root.openDetails(root.detailsEntryId)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                Layout.minimumHeight: root.compact ? 0 : 128
                spacing: MD.Token.spacing.small

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.extra_small

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.displayTitle
                        typescale: root.addonRow
                                   ? MD.Token.typescale.body_large
                                   : MD.Token.typescale.title_medium
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.WordWrap
                    }

                    MD.IconButton {
                        visible: root.inProgress && !root.isInstalling
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: root.isPaused ? MD.Token.icon.play_arrow : MD.Token.icon.pause
                        onClicked: Core.toggleJobPause(root.jobId)
                    }

                    MD.IconButton {
                        visible: root.inProgress && !root.isInstalling
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
                        visible: root.canRetryInstall
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.install_desktop
                        onClicked: Core.retryInstall(root.jobId)
                    }

                    MD.IconButton {
                        visible: root.canManualInstall
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.folder_open
                        onClicked: Core.confirmManualInstall(root.jobId)
                    }

                    MD.IconButton {
                        visible: root.isTerminal && (!root.embedded || root.showExternalRemove)
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.delete
                        onClicked: {
                            if (root.embedded && root.showExternalRemove)
                                root.removeRequested()
                            else
                                Core.removeJob(root.jobId)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    Rectangle {
                        Layout.preferredHeight: chipLabel.implicitHeight + 4
                        Layout.preferredWidth: chipLabel.implicitWidth + 12
                        radius: height / 2
                        color: MD.Util.transparent(root.statusAccent, 0.16)

                        MD.Label {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: root.statusChipText
                            typescale: MD.Token.typescale.label_small
                            color: root.statusAccent
                        }
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        visible: root.secondaryLine.length > 0
                        text: root.secondaryLine
                        color: (root.isFailed || root.installFailed)
                               ? MD.Token.color.error
                               : MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_medium
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                }

                DownloadJobProgressTrack {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    page: root
                }
            }
        }
    }
}
