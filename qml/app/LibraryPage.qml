import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int minCardWidth: 160
    readonly property int gridSpacing: MD.Token.spacing.medium
    readonly property int metaHeight: 48
    readonly property bool libraryEmpty: Core.library.count === 0
    readonly property int pageMargin: MD.Token.spacing.medium
    readonly property int cardRadius: MD.Token.shape.corner.extra_large

    property string selectedSourceId: Core.sources.firstEnabledId

    readonly property var heroGame: {
        if (Core.gameRunning && Core.runningGameId.length)
            return Core.library.gameInfo(Core.runningGameId)
        return Core.library.mostRecentGame()
    }

    readonly property bool featuredIsRunning: Core.gameRunning
        && Core.runningGameId === (root.heroGame?.gameId ?? "")
    readonly property bool showRunningHero: Core.gameRunning
    readonly property bool heroHasRecentPlay: root.showRunningHero
        || !!(root.heroGame?.lastPlayedAt?.length)
    readonly property string heroEyebrow: showRunningHero
        ? qsTr("Playing now")
        : qsTr("Recently played")
    readonly property string heroTitle: {
        if (root.showRunningHero || root.heroHasRecentPlay)
            return root.heroGame?.title ?? ""
        return qsTr("Nothing played yet")
    }
    readonly property string heroCoverUrl: root.heroGame?.coverUrl ?? ""
    readonly property string heroGameId: root.heroGame?.gameId ?? ""
    readonly property string heroSubtitle: {
        if (root.showRunningHero || root.heroHasRecentPlay)
            return (root.heroGame?.sourceName ?? "") + " · v" + (root.heroGame?.version ?? "")
        return qsTr("Launch a game from your library — it will appear here.")
    }
    readonly property bool heroHasUpdate: !!(root.heroGame?.hasUpdate)

    property int jobRevision: 0

    readonly property var featuredJob: {
        root.jobRevision
        if (!root.heroGameId.length)
            return ({})
        return Core.jobs.jobForEntry(root.heroGameId)
    }
    readonly property bool featuredShowJobStatus: root.heroGameId.length
        && !Core.isEntryPlayable(root.heroGameId)
        && !!(featuredJob.jobId)
        && !((root.heroGame?.installPath ?? "").length)
        && (featuredJob.inProgress
            || featuredJob.status === "installing"
            || featuredJob.status === "completed")
    readonly property real featuredFillProgress: {
        if (!featuredShowJobStatus)
            return -1
        if (featuredJob.inProgress || featuredJob.status === "installing")
            return featuredJob.progress
        return 100
    }
    readonly property string featuredStatusLine: {
        if (!featuredShowJobStatus)
            return ""
        if (featuredJob.status === "installing") {
            if (featuredJob.detail && featuredJob.detail.length)
                return featuredJob.detail
            return qsTr("Installing %1%").arg(featuredJob.progress)
        }
        if (featuredJob.status === "completed" && !featuredJob.inProgress)
            return qsTr("Installing…")
        if (featuredJob.status === "paused")
            return qsTr("Paused · %1%").arg(featuredJob.progress)
        return qsTr("Downloading %1%").arg(featuredJob.progress)
    }

    signal openGame(string gameId)
    signal openCatalog()
    signal openDownloads()
    signal openSettings()
    signal addSourceRequested()

    Component.onCompleted: {
        Core.prefetchCatalogCounts()
    }

    Connections {
        target: Core.jobs
        function onJobsChanged() { root.jobRevision++ }
    }

    Connections {
        target: Core.library
        function onLibraryChanged() { /* heroGame binding refreshes */ }
    }


    LibraryEmptyState {
        anchors.fill: parent
        page: root
    }

    LibraryContent {
        anchors.fill: parent
        page: root
    }
}
