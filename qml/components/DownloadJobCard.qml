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

    readonly property string detailsEntryId: root.parentEntryId.length
        ? root.parentEntryId
        : root.entryId
    readonly property real coverColumnWidth: root.compact ? 0 : 88

    signal openDetails(string entryId)

    readonly property bool inProgress: ["starting", "checking", "metadata", "downloading", "seeding", "paused", "installing"].includes(status)
    readonly property bool isInstalling: status === "installing"
    readonly property bool isPaused: status === "paused"
    readonly property bool isFailed: status === "failed" || status === "cancelled"
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

    function isStatusDetail(d) {
        if (!d || d.length === 0)
            return true
        return d === "Downloading…"
                || d === "Загрузка…"
                || d.indexOf("Preparing") >= 0
                || d.indexOf("Подготовка") >= 0
                || d.indexOf("Getting game info") >= 0
                || d.indexOf("Installed") >= 0
                || d.indexOf("Установлено") >= 0
    }

    readonly property string transferLine: {
        const d = (root.detail || "").trim()
        // Core already builds "2.7 GB · 12 MB/s" — use it when it's not a status phrase.
        if (d.length > 0 && !isStatusDetail(d))
            return d
        if (d.length > 0 && isStatusDetail(d)
                && d !== "Downloading…"
                && d !== "Загрузка…")
            return d

        const downloaded = root.effectiveDownloaded
        const total = root.effectiveTotalBytes
        const speed = formatSpeed(root.estimatedRateBps)
        if (total > 0) {
            const base = formatByteCount(downloaded) + " / " + formatByteCount(total)
            const eta = formatEta(Math.max(0, total - downloaded), root.estimatedRateBps)
            if (speed && eta)
                return base + " · " + speed + " · ETA " + eta
            if (speed)
                return base + " · " + speed
            return base
        }
        if (downloaded > 0 && speed)
            return formatByteCount(downloaded) + " · " + speed
        if (downloaded > 0)
            return formatByteCount(downloaded)
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
        id: rateTimer
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

    readonly property string progressLabel: (root.inProgress || status === "completed")
            && root.effectiveTotalBytes > 0
        ? formatByteCount(root.effectiveDownloaded) + " / " + formatByteCount(root.effectiveTotalBytes)
        : root.progress + "%"

    implicitWidth: embedded ? parent ? parent.width : implicitWidth : implicitWidth
    implicitHeight: content.implicitHeight + (embedded ? 0 : 2 * MD.Token.spacing.medium)

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
        anchors.margins: root.embedded ? (root.compact ? MD.Token.spacing.small : 0) : MD.Token.spacing.medium
        implicitHeight: row.implicitHeight + (root.addonRow ? MD.Token.spacing.small : 0)

        RowLayout {
            id: row
            width: parent.width
            spacing: root.addonRow ? MD.Token.spacing.small : (root.compact ? MD.Token.spacing.small : MD.Token.spacing.medium)

            MD.Icon {
                visible: root.addonRow
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 2
                name: MD.Token.icon.downloading
                size: 16
                color: root.inProgress ? MD.Token.color.primary : MD.Token.color.on_surface_variant
            }

            Item {
                visible: !root.compact
                Layout.preferredWidth: root.coverColumnWidth
                Layout.preferredHeight: 118
                Layout.alignment: Qt.AlignTop
                z: 1

                GamePoster {
                    anchors.fill: parent
                    source: root.coverUrl
                    seed: root.title
                    fallbackText: root.title.charAt(0)
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
                Layout.alignment: Qt.AlignTop
                spacing: MD.Token.spacing.small

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.title
                        typescale: root.addonRow
                                 ? MD.Token.typescale.body_medium
                                 : root.compact
                                   ? MD.Token.typescale.body_large
                                   : MD.Token.typescale.title_small
                        elide: Text.ElideRight
                        maximumLineCount: root.compact ? 2 : 1
                        wrapMode: root.compact ? Text.WordWrap : Text.NoWrap
                    }

                    MD.Label {
                        visible: (root.inProgress || status === "completed") && !root.isInstalling
                        text: root.progressLabel
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.primary
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

                Item {
                    id: progressTrack
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.addonRow ? 3 : 4
                    clip: true

                    Rectangle {
                        anchors.fill: parent
                        radius: 2
                        color: MD.Util.transparent(MD.Token.color.primary, 0.25)
                    }

                    Rectangle {
                        id: installIndeterminate
                        visible: root.isInstalling
                        height: parent.height
                        width: Math.max(24, parent.width * 0.35)
                        radius: 2
                        color: MD.Token.color.primary
                        x: -width

                        SequentialAnimation {
                            running: root.isInstalling
                            loops: Animation.Infinite
                            NumberAnimation {
                                target: installIndeterminate
                                property: "x"
                                from: -installIndeterminate.width
                                to: progressTrack.width
                                duration: 900
                                easing.type: Easing.InOutQuad
                            }
                            NumberAnimation {
                                target: installIndeterminate
                                property: "x"
                                from: progressTrack.width
                                to: -installIndeterminate.width
                                duration: 900
                                easing.type: Easing.InOutQuad
                            }
                        }
                    }

                    Rectangle {
                        visible: !root.isInstalling
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
                        visible: root.transferLine.length > 0
                        text: root.transferLine
                        color: root.isFailed || root.installFailed
                               ? MD.Token.color.error
                               : MD.Token.color.primary
                        typescale: MD.Token.typescale.label_large
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        visible: root.transferLine.length === 0
                                 && ((root.detail.length ? root.detail : root.statusLabel).length > 0)
                        text: root.detail.length ? root.detail : root.statusLabel
                        color: (root.isFailed || root.installFailed) ? MD.Token.color.error
                                                                       : MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_medium
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    MD.Label {
                        visible: root.statusLabel.length > 0 && root.detail.length > 0 && !root.isInstalling
                        text: root.statusLabel
                        color: root.installFailed ? MD.Token.color.error
                                                    : MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_small
                    }
                }
            }
        }
    }
}
