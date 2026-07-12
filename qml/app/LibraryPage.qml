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
    readonly property int pageMargin: MD.Token.spacing.large
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
    readonly property string heroEyebrow: showRunningHero
        ? qsTr("Playing now")
        : qsTr("Recently played")
    readonly property string heroTitle: root.heroGame?.title ?? ""
    readonly property string heroCoverUrl: root.heroGame?.coverUrl ?? ""
    readonly property string heroGameId: root.heroGame?.gameId ?? ""
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

    // ── Empty library ────────────────────────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: root.libraryEmpty

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: root.pageMargin
            anchors.rightMargin: root.pageMargin
            spacing: MD.Token.spacing.large

            Item {
                id: emptyHero
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                clip: true

                layer.enabled: true
                layer.effect: MD.RoundClip {
                    corners: MD.Util.corners(root.cardRadius)
                    size: Qt.vector2d(emptyHero.width, emptyHero.height)
                }

                Rectangle {
                    anchors.fill: parent
                    color: MD.Token.color.surface_container_high
                }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: MD.Util.transparent(MD.Token.color.primary, 0.12)
                        }
                        GradientStop {
                            position: 1.0
                            color: "transparent"
                        }
                    }
                }

                SpiderWebMark {
                    width: 340
                    height: 340
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: -170
                    strokeColor: MD.Token.color.primary
                    strokeWidth: 3
                    opacity: 0.20
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: MD.Token.spacing.extra_large
                    anchors.rightMargin: MD.Token.spacing.extra_large + 120
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Nothing here yet")
                        typescale: MD.Token.typescale.headline_medium
                        wrapMode: Text.WordWrap
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 520
                        text: qsTr("Your library is empty. Install a source plugin, pick a game in the catalog, and it will show up here.")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.topMargin: MD.Token.spacing.small
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            text: Core.sources.enabledCount > 0
                                  ? qsTr("Open catalog")
                                  : qsTr("Install plugin")
                            icon.name: Core.sources.enabledCount > 0
                                       ? MD.Token.icon.storefront
                                       : MD.Token.icon.add
                            mdState.type: MD.Enum.BtFilled
                            onClicked: {
                                if (Core.sources.enabledCount > 0)
                                    root.openCatalog()
                                else
                                    root.addSourceRequested()
                            }
                        }

                        MD.Button {
                            visible: Core.sources.enabledCount > 0
                            text: qsTr("Settings")
                            icon.name: MD.Token.icon.settings
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: root.openSettings()
                        }

                        MD.Button {
                            visible: Core.sources.enabledCount === 0
                            text: qsTr("Catalogs and plugins")
                            mdState.type: MD.Enum.BtText
                            onClicked: sourceHelpDialog.open()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.medium

                Repeater {
                    model: [
                        {
                            icon: MD.Token.icon.extension,
                            step: qsTr("Step 1"),
                            title: qsTr("Plugin"),
                            body: qsTr("Install a source plugin (FreeTP, etc.) under Settings → Plugins.")
                        },
                        {
                            icon: MD.Token.icon.storefront,
                            step: qsTr("Step 2"),
                            title: qsTr("Catalog"),
                            body: qsTr("Pick a game and start installation — the torrent downloads automatically.")
                        },
                        {
                            icon: MD.Token.icon.sports_esports,
                            step: qsTr("Step 3"),
                            title: qsTr("Library"),
                            body: qsTr("Installed games live here: launch, updates, and details.")
                        }
                    ]

                    Item {
                        id: tipHost
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: tipInner.implicitHeight + MD.Token.spacing.large * 2

                        Item {
                            id: tipCard
                            anchors.fill: parent
                            clip: true

                            layer.enabled: true
                            layer.effect: MD.RoundClip {
                                corners: MD.Util.corners(root.cardRadius)
                                size: Qt.vector2d(tipCard.width, tipCard.height)
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: MD.Token.color.surface_container
                            }

                            ColumnLayout {
                                id: tipInner
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: MD.Token.spacing.large
                                spacing: MD.Token.spacing.small

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: MD.Token.spacing.medium

                                    Rectangle {
                                        Layout.preferredWidth: 48
                                        Layout.preferredHeight: 48
                                        radius: MD.Token.shape.corner.medium
                                        color: MD.Token.color.secondary_container

                                        MD.Icon {
                                            anchors.centerIn: parent
                                            name: tipHost.modelData.icon
                                            size: 24
                                            color: MD.Token.color.on_secondary_container
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        MD.Label {
                                            Layout.fillWidth: true
                                            text: tipHost.modelData.step
                                            color: MD.Token.color.primary
                                            typescale: MD.Token.typescale.label_medium
                                        }

                                        MD.Label {
                                            Layout.fillWidth: true
                                            text: tipHost.modelData.title
                                            typescale: MD.Token.typescale.title_small
                                        }
                                    }
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: tipHost.modelData.body
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_medium
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    SourceHelpDialog {
        id: sourceHelpDialog
    }

    // ── Populated library ────────────────────────────────────────────────────
    Flickable {
        id: flick
        anchors.fill: parent
        visible: !root.libraryEmpty
        contentWidth: width
        contentHeight: contentCol.implicitHeight + MD.Token.spacing.large
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: flick.width
            spacing: MD.Token.spacing.large

            Item {
                id: heroHost
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                Layout.topMargin: MD.Token.spacing.medium
                Layout.preferredHeight: 280

                Item {
                    id: heroCard
                    anchors.fill: parent
                    clip: true

                    layer.enabled: true
                    layer.effect: MD.RoundClip {
                        corners: MD.Util.corners(root.cardRadius)
                        size: Qt.vector2d(heroCard.width, heroCard.height)
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: MD.Token.color.surface_container
                    }

                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: MD.Util.transparent(MD.Token.color.primary, 0.10)
                            }
                            GradientStop {
                                position: 1.0
                                color: "transparent"
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: MD.Token.spacing.large
                        spacing: MD.Token.spacing.large

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: MD.Token.spacing.small

                            MD.Label {
                                Layout.fillWidth: true
                                text: root.heroEyebrow
                                color: MD.Token.color.primary
                                typescale: MD.Token.typescale.label_large
                                elide: Text.ElideRight
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: root.heroTitle
                                typescale: MD.Token.typescale.headline_large
                                elide: Text.ElideRight
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: (root.heroGame?.sourceName ?? "") + " · v" + (root.heroGame?.version ?? "")
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.body_large
                                elide: Text.ElideRight
                                visible: !root.featuredShowJobStatus && !root.showRunningHero
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.extra_small
                                visible: root.showRunningHero

                                MD.Icon {
                                    name: MD.Token.icon.sports_esports
                                    size: 18
                                    color: MD.Token.color.primary
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Running")
                                    color: MD.Token.color.primary
                                    typescale: MD.Token.typescale.body_large
                                    elide: Text.ElideRight
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.extra_small
                                visible: root.featuredShowJobStatus

                                MD.Icon {
                                    name: root.featuredJob.status === "installing"
                                          ? MD.Token.icon.install_desktop
                                          : root.featuredJob.status === "paused"
                                            ? MD.Token.icon.play_arrow
                                            : MD.Token.icon.pause
                                    size: 18
                                    color: MD.Token.color.on_surface_variant
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: root.featuredStatusLine
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_large
                                    elide: Text.ElideRight
                                }
                            }

                            Item { Layout.fillHeight: true }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.small

                                MD.Button {
                                    visible: !root.showRunningHero
                                    text: qsTr("Play")
                                    mdState.type: MD.Enum.BtFilled
                                    enabled: !!(root.heroGameId)
                                             && Core.isEntryPlayable(root.heroGameId)
                                    onClicked: Core.launchGame(root.heroGameId)
                                }

                                MD.Button {
                                    text: qsTr("Details")
                                    mdState.type: MD.Enum.BtOutlined
                                    enabled: !!(root.heroGameId)
                                    onClicked: root.openGame(root.heroGameId)
                                }

                                MD.Button {
                                    visible: root.heroHasUpdate && !root.showRunningHero
                                    text: qsTr("Refresh")
                                    icon.name: MD.Token.icon.update
                                    mdState.type: MD.Enum.BtFilledTonal
                                    enabled: !!(root.heroGameId)
                                    onClicked: Core.updateCatalogEntry(root.heroGameId)
                                }
                            }
                        }

                        GamePoster {
                            Layout.preferredWidth: 140
                            Layout.preferredHeight: 186
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                            source: root.heroCoverUrl
                            seed: root.heroTitle
                            fallbackText: (root.heroTitle || "?").charAt(0)
                            cornerRadius: root.cardRadius
                            fillProgress: root.featuredShowJobStatus && !root.showRunningHero
                                          ? root.featuredFillProgress
                                          : -1
                            onClicked: {
                                if (root.heroGameId)
                                    root.openGame(root.heroGameId)
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                spacing: MD.Token.spacing.medium

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("In library")
                    value: String(Core.library.count)
                    iconName: MD.Token.icon.sports_esports
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Sources")
                    value: String(Core.sources.enabledCount)
                    iconName: MD.Token.icon.storefront
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Tasks")
                    value: String(Core.jobs.count)
                    iconName: MD.Token.icon.downloading
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Updates")
                    value: String(Core.library.updateCount())
                    iconName: MD.Token.icon.update
                }
            }

            MD.Pane {
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                visible: Core.jobs.activeCount > 0
                padding: MD.Token.spacing.medium
                radius: root.cardRadius
                backgroundColor: MD.Token.color.secondary_container
                clip: true

                RowLayout {
                    anchors.fill: parent
                    spacing: MD.Token.spacing.medium

                    MD.Icon {
                        name: MD.Token.icon.downloading
                        size: 24
                        color: MD.Token.color.on_secondary_container
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("%1 active downloads").arg(Core.jobs.activeCount)
                            color: MD.Token.color.on_secondary_container
                            typescale: MD.Token.typescale.title_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Downloads continue after restart")
                            color: MD.Token.color.on_secondary_container
                            typescale: MD.Token.typescale.label_medium
                        }
                    }

                    MD.Button {
                        text: qsTr("Open")
                        onClicked: root.openDownloads()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("My library")
                    typescale: MD.Token.typescale.title_large
                }

                MD.Label {
                    text: qsTr("%1 games").arg(Core.library.count)
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_large
                }
            }

            Item {
                id: gridHost
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin

                property int columns: 3
                property real cardWidth: 160
                property real cardHeight: 260

                function relayout() {
                    const cols = Math.max(2, Math.floor((width + root.gridSpacing) / (root.minCardWidth + root.gridSpacing)))
                    const cardW = (width - root.gridSpacing * (cols - 1)) / cols
                    columns = cols
                    cardWidth = cardW
                    cardHeight = cardW * 4 / 3 + root.metaHeight
                    const rows = Math.max(1, Math.ceil(Core.library.count / cols))
                    Layout.preferredHeight = rows * (cardHeight + root.gridSpacing)
                }

                Timer {
                    id: layoutTimer
                    interval: 50
                    onTriggered: gridHost.relayout()
                }

                onWidthChanged: layoutTimer.restart()
                Component.onCompleted: relayout()

                Connections {
                    target: Core.library
                    function onModelReset() { gridHost.relayout() }
                    function onRowsInserted() { gridHost.relayout() }
                    function onRowsRemoved() { gridHost.relayout() }
                }

                GridView {
                    anchors.fill: parent
                    clip: true
                    interactive: false
                    model: Core.library
                    cellWidth: gridHost.cardWidth + root.gridSpacing
                    cellHeight: gridHost.cardHeight + root.gridSpacing
                    cacheBuffer: 0
                    delegate: LibraryGameCard {
                        width: Math.max(0, gridHost.cardWidth - root.gridSpacing)
                        height: gridHost.cardHeight
                        onOpenDetails: function (id) { root.openGame(id) }
                    }
                }
            }
        }
    }
}
