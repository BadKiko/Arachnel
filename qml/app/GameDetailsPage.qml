import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    property string gameId: ""
    property bool fromCatalog: false

    // Opaque surface so catalog doesn't ghost through during page fade/bounce.
    Rectangle {
        anchors.fill: parent
        color: MD.Token.color.surface
    }

    property int detailsRevision: 0
    property bool mediaLoading: false

    readonly property bool hasCachedMedia: {
        const _rev = root.detailsRevision
        const shots = root.info.screenshotUrls ?? []
        return shots.length > 0 || ((root.info.trailerUrl ?? "")).length > 0
    }

    function syncMediaLoading() {
        root.mediaLoading = !root.hasCachedMedia
    }

    readonly property var info: {
        const _rev = root.detailsRevision
        return gameId.length ? Core.entryDetails(gameId) : ({})
    }

    Connections {
        target: Core
        function onEntryMetadataChanged(entryId) {
            if (entryId === root.gameId) {
                root.mediaLoading = false
                root.detailsRevision++
            }
        }
    }

    Connections {
        target: Core.library
        function onLibraryChanged() { root.detailsRevision++ }
    }

    Connections {
        target: Core
        function onPluginsChanged() { root.detailsRevision++ }
    }

    readonly property bool playable: {
        const _rev = root.detailsRevision
        return Core.isEntryPlayable(gameId)
    }
    readonly property bool installed: {
        const _rev = root.detailsRevision
        if (root.playable)
            return true
        if (!gameId.length)
            return false
        const lib = Core.library.gameInfo(gameId)
        return ((lib.installPath ?? "")).length > 0
    }
    readonly property bool inLibrary: {
        const _rev = root.detailsRevision
        if (!gameId.length)
            return false
        const lib = Core.library.gameInfo(gameId)
        return (lib.gameId ?? "").length > 0
    }
    readonly property bool isRunning: Core.gameRunning && Core.runningGameId === root.gameId
    readonly property bool runtimeSetupActive: Core.runtimeSetupInProgress
        && Core.runtimeSetupGameId === root.gameId
    readonly property bool onLinux: Qt.platform.os === "linux"
    readonly property bool downloadFilesExist: {
        const _rev = root.detailsRevision
        return Core.entryDownloadFilesExist(gameId)
    }
    readonly property bool downloadComplete: {
        const _rev = root.detailsRevision
        return Core.isEntryDownloadComplete(gameId)
    }
    readonly property bool installFailed: (downloadJob.detail || "").indexOf("Install failed") >= 0
    readonly property bool isInstalling: downloadJob.status === "installing"
    readonly property bool readyToInstall: !root.playable
        && !root.installed
        && downloadJob.status === "completed"
        && root.downloadFilesExist
        && !root.installFailed
    readonly property bool canManageDownload: !root.playable && (
        root.fromCatalog
        || root.inLibrary
        || root.showDownloadProgress
        || root.readyToInstall
        || root.installFailed
    )

    property var downloadJob: ({})

    readonly property bool downloadPaused: downloadJob.status === "paused" || !!downloadJob.paused
    readonly property bool downloadActive: !!(downloadJob.inProgress) && !downloadPaused
    readonly property bool downloadCompleted: downloadJob.status === "completed"
    readonly property bool showDownloadProgress: !!(downloadJob.inProgress) || downloadCompleted

    function refreshDownloadJob() {
        downloadJob = Core.jobs.jobForEntry(root.gameId)
    }

    function parseSizeLabelBytes(label) {
        if (!label || !label.length)
            return 0
        const m = /^(\d+(?:\.\d+)?)\s*(B|KB|MB|GB|TB)/i.exec(label.trim())
        if (!m)
            return 0
        let v = parseFloat(m[1])
        const unit = m[2].toUpperCase()
        if (unit === "KB")
            v *= 1024
        else if (unit === "MB")
            v *= 1024 * 1024
        else if (unit === "GB")
            v *= 1024 * 1024 * 1024
        else if (unit === "TB")
            v *= 1024 * 1024 * 1024 * 1024
        return v
    }

    readonly property real effectiveTotalBytes: {
        const jobTotal = root.downloadJob.totalBytes ?? 0
        if (jobTotal > 0)
            return jobTotal
        return root.parseSizeLabelBytes(root.info.sizeLabel ?? "")
    }

    readonly property real effectiveDownloaded: {
        const raw = root.downloadJob.bytesDownloaded ?? 0
        const total = root.effectiveTotalBytes
        if (raw > 0)
            return raw
        if (total > 0 && (root.downloadJob.progress ?? 0) > 0)
            return total * root.downloadJob.progress / 100
        return 0
    }

    readonly property real downloadTotalBytes: root.effectiveTotalBytes

    Connections {
        target: Core.jobs
        function onJobsChanged() {
            root.refreshDownloadJob()
            root.detailsRevision++
        }
    }

    onGameIdChanged: {
        refreshDownloadJob()
        syncMediaLoading()
        maybeEnrich()
        if (root.onLinux)
            Core.refreshAvailableProtons()
    }
    onFromCatalogChanged: maybeEnrich()

    function maybeEnrich() {
        if (gameId.length > 0) {
            if (!root.hasCachedMedia)
                root.mediaLoading = true
            Core.enrichCatalogEntry(gameId)
        }
    }

    Timer {
        id: mediaLoadTimeout
        interval: 20000
        repeat: false
        onTriggered: root.mediaLoading = false
    }

    onMediaLoadingChanged: {
        if (root.mediaLoading)
            mediaLoadTimeout.restart()
        else
            mediaLoadTimeout.stop()
    }

    Component.onCompleted: {
        refreshDownloadJob()
        syncMediaLoading()
        maybeEnrich()
    }

    readonly property string sourceLabel: {
        const sid = info.sourceId ?? ""
        if (!sid.length)
            return ""
        if (info.sourceName && info.sourceName.length && info.sourceName !== sid)
            return info.sourceName
        return Core.sources.nameForId(sid)
    }

    signal backRequested()
    signal openAddonPicker(string entryId, string title)
    signal openInstallPicker(string entryId, string title, var selectedAddonIds)
    signal protonRequired()

    function needsProtonCheck() {
        return root.onLinux && Core.needsProtonOnPlatform() && !Core.protonReady
    }

    function proceedToInstall(selectedAddonIds) {
        if (root.needsProtonCheck()) {
            root.protonRequired()
            return
        }
        const ids = selectedAddonIds || []
        if (Core.needsInstallLocationChoice())
            root.openInstallPicker(root.gameId, root.info.title || "", ids)
        else
            Core.installCatalogEntry(root.gameId, "", ids)
    }

    function beginInstall() {
        if (root.needsProtonCheck()) {
            root.protonRequired()
            return
        }
        const addonCount = root.info.addonCount ?? 0
        if (addonCount > 0)
            root.openAddonPicker(root.gameId, root.info.title || "")
        else
            root.proceedToInstall([])
    }

    function confirmRemove() {
        Core.removeEntry(root.gameId, true)
        root.backRequested()
    }


    GameDetailsContent {
        anchors.fill: parent
        page: root
    }
}
