import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

import Qcm.Material as MD

Item {
    id: root

    property int progress: 0
    property real bytesDownloaded: 0
    property real totalBytes: 0
    property bool paused: false
    property bool downloading: false
    property bool completed: false
    property bool readyToInstall: false
    property bool installFailed: false
    property bool installing: false
    property string idleText: qsTr("Download")
    property string detail: ""

    readonly property bool inProgress: root.downloading || root.paused

    property real _lastBytesSample: 0
    property real _lastBytesAtMs: 0
    property real estimatedRateBps: 0

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
    }

    readonly property string transferLine: {
        const d = (root.detail || "").trim()
        if (d.length > 0 && !isStatusDetail(d))
            return d
        if (d.length > 0 && isStatusDetail(d)
                && d !== "Downloading…"
                && d !== "Загрузка…")
            return d

        const downloaded = root.bytesDownloaded > 0
            ? root.bytesDownloaded
            : (root.totalBytes > 0 && root.progress > 0
                ? root.totalBytes * root.progress / 100
                : 0)
        const total = root.totalBytes || 0
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

    Timer {
        id: rateTimer
        interval: 250
        running: root.inProgress && root.downloading
        repeat: true
        onTriggered: {
            const downloaded = root.bytesDownloaded > 0
                ? root.bytesDownloaded
                : (root.totalBytes > 0 && root.progress > 0
                    ? root.totalBytes * root.progress / 100
                    : 0)
            const now = Date.now()
            if (_lastBytesAtMs > 0 && downloaded > _lastBytesSample) {
                const dt = (now - _lastBytesAtMs) / 1000
                const delta = downloaded - _lastBytesSample
                if (dt > 0.4 && delta > 0)
                    root.estimatedRateBps = delta / dt
            }
            _lastBytesSample = downloaded
            _lastBytesAtMs = now
        }
    }

    onDownloadingChanged: {
        if (!root.downloading) {
            _lastBytesSample = 0
            _lastBytesAtMs = 0
            estimatedRateBps = 0
        }
    }

    readonly property real fillRatio: Math.max(0, Math.min(1, root.progress / 100))
    readonly property bool showProgressFill: root.inProgress && !root.completed && !root.readyToInstall
        && !root.installing
    readonly property real pillRadius: shell.height / 2

    readonly property string actionIconName: root.installing
        ? MD.Token.icon.install_desktop
        : root.installFailed
        ? MD.Token.icon.refresh
        : root.readyToInstall
          ? MD.Token.icon.install_desktop
          : root.completed
            ? MD.Token.icon.check_circle
            : root.paused
              ? MD.Token.icon.play_arrow
              : root.inProgress
                ? MD.Token.icon.pause
                : MD.Token.icon.download

    readonly property string actionText: root.installing
        ? qsTr("Installing…")
        : root.installFailed
        ? qsTr("Retry install")
        : root.readyToInstall
          ? qsTr("Install")
          : root.completed
            ? qsTr("Downloaded")
            : root.paused
              ? qsTr("Paused · %1%").arg(root.progress)
              : root.inProgress
                ? qsTr("Downloading · %1%").arg(root.progress)
                : root.idleText

    readonly property color shellColor: root.installing
        ? MD.Token.color.primary_container
        : root.completed || root.readyToInstall
        ? MD.Token.color.secondary_container
        : root.installFailed
          ? MD.Util.transparent(MD.Token.color.error, 0.18)
          : root.showProgressFill
            ? MD.Token.color.primary_container
            : MD.Token.color.primary

    readonly property color labelColor: root.installing
        ? MD.Token.color.on_primary_container
        : root.installFailed
        ? MD.Token.color.error
        : root.completed || root.readyToInstall
          ? MD.Token.color.on_secondary_container
          : root.showProgressFill
            ? MD.Token.color.on_primary_container
            : MD.Token.color.on_primary

    signal activated()
    signal pauseToggleRequested()
    signal cancelRequested()

    component ActionRow : RowLayout {
        id: row
        required property color tone
        spacing: MD.Token.spacing.small

        MD.Icon {
            name: root.actionIconName
            size: 18
            color: row.tone
        }

        MD.Label {
            text: root.actionText
            typescale: MD.Token.typescale.label_large
            color: row.tone
        }
    }

    implicitWidth: Math.max(shell.width, detailLabel.implicitWidth)
        + (cancelButton.visible ? cancelButton.implicitWidth + MD.Token.spacing.small : 0)
    implicitHeight: shell.height + (detailLabel.visible ? detailLabel.implicitHeight + MD.Token.spacing.extra_small : 0)

    Column {
        spacing: MD.Token.spacing.extra_small

        Row {
            spacing: MD.Token.spacing.small
            height: 40

            Item {
                id: shell
                width: Math.max(196, labelRow.implicitWidth + 2 * MD.Token.spacing.large)
                height: 40
                clip: true

                layer.enabled: true
                layer.effect: MD.RoundClip {
                    corners: MD.Util.corners(root.pillRadius)
                    size: Qt.vector2d(shell.width, shell.height)
                }

                Rectangle {
                    anchors.fill: parent
                    color: root.shellColor
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: parent.width * root.fillRatio
                    visible: root.showProgressFill
                    color: MD.Token.color.primary
                }

                ActionRow {
                    id: labelRow
                    anchors.centerIn: parent
                    tone: root.labelColor
                }

                Item {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: parent.width * root.fillRatio
                    clip: true
                    visible: root.showProgressFill

                    ActionRow {
                        x: labelRow.x
                        y: labelRow.y
                        tone: MD.Token.color.on_primary
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: root.installing ? Qt.BusyCursor : Qt.PointingHandCursor
                    enabled: !root.completed && !root.installing
                    onClicked: {
                        if (root.inProgress)
                            root.pauseToggleRequested()
                        else
                            root.activated()
                    }
                }
            }

            MD.IconButton {
                id: cancelButton
                visible: root.inProgress && !root.completed
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.close
                onClicked: root.cancelRequested()
            }
        }

        MD.Label {
            id: detailLabel
            width: Math.max(shell.width, implicitWidth)
            visible: root.inProgress && !root.installing && root.transferLine.length > 0
            text: root.transferLine
            color: MD.Token.color.primary
            typescale: MD.Token.typescale.label_large
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }
}
