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

    readonly property bool gameFound: {
        const _rev = root.detailsRevision
        if (!root.gameId.length)
            return false
        return ((root.info.title ?? "")).toString().trim().length > 0
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
        let job = Core.jobs.jobForEntry(root.gameId)
        if (job && job.jobId) {
            downloadJob = job
            return
        }
        if (root.pendingInstallEntryId.length) {
            job = Core.jobs.jobForEntry(root.pendingInstallEntryId)
            if (job && job.jobId) {
                downloadJob = job
                return
            }
        }
        const offers = Core.installOffersForEntry(root.gameId)
        if (offers && offers.length) {
            for (let i = 0; i < offers.length; ++i) {
                const oid = offers[i].entryId || ""
                if (!oid.length || oid === root.gameId)
                    continue
                job = Core.jobs.jobForEntry(oid)
                if (job && job.jobId) {
                    downloadJob = job
                    return
                }
            }
        }
        downloadJob = job || ({})
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
            const prevStatus = root.downloadJob.status || ""
            root.refreshDownloadJob()
            // Progress ticks used to bump detailsRevision every time, which rebuilt
            // the screenshot strip and made tiles vanish mid-download.
            if ((root.downloadJob.status || "") !== prevStatus)
                root.detailsRevision++
        }
    }

    onGameIdChanged: {
        root.pendingInstallEntryId = ""
        root.pendingInstallSourceId = ""
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

    readonly property int installSourceCount: {
        const _rev = root.detailsRevision
        if (!root.gameId.length)
            return 0
        const offers = Core.installOffersForEntry(root.gameId)
        if (!offers || !offers.length)
            return 0
        const seen = ({})
        let n = 0
        for (let i = 0; i < offers.length; ++i) {
            const sid = offers[i].sourceId || ""
            if (!sid.length || seen[sid])
                continue
            seen[sid] = true
            ++n
        }
        return n
    }

    readonly property string sourceLabel: {
        const _rev = root.detailsRevision
        // Merged catalog entry available from several plugins - don't pin one name.
        if (root.installSourceCount > 1)
            return qsTr("%n source(s)", "", root.installSourceCount)
        const sid = info.sourceId ?? ""
        if (!sid.length)
            return ""
        if (info.sourceName && info.sourceName.length && info.sourceName !== sid)
            return info.sourceName
        return Core.sources.nameForId(sid)
    }

    signal backRequested()
    signal openSourcePicker(string entryId, string title)
    signal openAddonPicker(string entryId, string title)
    signal openInstallPicker(string entryId, string title, var selectedAddonIds, string sourceId)
    signal openSteamidraTrust()
    signal openSourcesRequested()
    signal protonRequired()

    /** Catalog entry id for the chosen install source (may differ from showcase gameId). */
    property string pendingInstallEntryId: ""
    property string pendingInstallSourceId: ""

    function resolveInstallEntryId() {
        return root.pendingInstallEntryId.length ? root.pendingInstallEntryId : root.gameId
    }

    function needsProtonCheck() {
        return root.onLinux && Core.needsProtonOnPlatform() && !Core.protonReady
    }

    function proceedToInstall(selectedAddonIds) {
        if (root.needsProtonCheck()) {
            root.protonRequired()
            return
        }
        const ids = selectedAddonIds || []
        const installId = root.resolveInstallEntryId()
        const details = Core.entryDetails(installId)
        const title = (details.title || root.info.title || "")
        const sourceId = root.pendingInstallSourceId || ""
        if (Core.needsInstallLocationChoice())
            root.openInstallPicker(installId, title, ids, sourceId)
        else if (sourceId.length)
            Core.installCatalogEntryFromSource(root.gameId, sourceId, "", ids)
        else
            Core.installCatalogEntry(installId, "", ids)
    }

    function continueAfterSourceChosen() {
        const installId = root.resolveInstallEntryId()
        const details = Core.entryDetails(installId)
        const title = details.title || root.info.title || ""

        function proceedWithAddons() {
            const refreshed = Core.entryDetails(installId)
            const addons = Core.catalog.addonsFor(installId)
            const addonCount = refreshed.addonCount ?? (addons ? addons.length : 0)
            if (addonCount > 0) {
                root.openAddonPicker(installId, title)
                return
            }
            root.afterAddonsSelected([])
        }

        if (!Core.ensureCatalogAddons) {
            proceedWithAddons()
            return
        }
        const ready = Core.ensureCatalogAddons(installId)
        if (ready) {
            proceedWithAddons()
            return
        }
        // Open picker immediately with loading - don't freeze the details page.
        root.openAddonPicker(installId, title)
    }

    function beginInstall() {
        if (root.needsProtonCheck()) {
            root.protonRequired()
            return
        }
        root.pendingInstallEntryId = ""
        root.pendingInstallSourceId = ""
        const offers = Core.installOffersForEntry(root.gameId)
        if (offers.length > 1) {
            root.openSourcePicker(root.gameId, root.info.title || "")
            return
        }
        if (offers.length === 1 && (offers[0].entryId || "").length) {
            root.pendingInstallEntryId = offers[0].entryId
            root.pendingInstallSourceId = offers[0].sourceId || ""
        } else {
            root.pendingInstallEntryId = root.gameId
            root.pendingInstallSourceId = root.info.sourceId || ""
        }
        root.continueAfterSourceChosen()
    }

    function afterSourceSelected(offerEntryId, sourceId) {
        root.pendingInstallEntryId = (offerEntryId && offerEntryId.length) ? offerEntryId : root.gameId
        root.pendingInstallSourceId = sourceId || ""
        root.continueAfterSourceChosen()
    }

    function afterAddonsSelected(selectedAddonIds) {
        root.proceedToInstall(selectedAddonIds || [])
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
