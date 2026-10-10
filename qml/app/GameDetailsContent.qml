import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: detailsRoot
    required property var page

    readonly property bool hasLaunchLog: {
        const _rev = page.detailsRevision
        return Core.hasGameLaunchLog(page.gameId)
    }

    function openLaunchLog() {
        launchLogDialog.openForGame(page.gameId)
    }

    // Eases the page to an item instead of jumping.
    function scrollToItem(item, margin) {
        if (!item)
            return
        const target = Math.max(0, Math.min(flick.contentHeight - flick.height,
                                            item.mapToItem(flick.contentItem, 0, 0).y - (margin ?? 80)))
        scrollAnim.stop()
        scrollAnim.to = target
        scrollAnim.start()
    }

    function scrollToTop() {
        scrollAnim.stop()
        scrollAnim.to = 0
        scrollAnim.start()
    }

    NumberAnimation {
        id: scrollAnim
        target: flick
        property: "contentY"
        duration: AppMotion.scaled(560)
        easing: AppMotion.emphasized
    }

    Connections {
        target: Core
        function onLaunchSessionEnded(gameId, elapsedMs, suppressQuickExitLog) {
            if (gameId !== page.gameId)
                return
            page.detailsRevision++
            // Core suppresses this for OF auto-retry and a clean user quit.
            if (suppressQuickExitLog)
                return
            if (elapsedMs >= 0 && elapsedMs < 20000)
                openLaunchLog()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !page.gameFound

            ColumnLayout {
                anchors.centerIn: parent
                spacing: MD.Token.spacing.medium
                width: Math.min(parent.width - MD.Token.spacing.large * 2, 420)

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
                    text: qsTr("Game not found")
                    typescale: MD.Token.typescale.title_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: Messages.gameNotFoundHint
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                    wrapMode: Text.WordWrap
                }

                MD.Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Open sources")
                    icon.name: MD.Token.icon.storefront
                    mdState.type: MD.Enum.BtFilled
                    onClicked: page.openSourcesRequested()
                }
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.gameFound
            contentWidth: width
            contentHeight: contentCol.implicitHeight + MD.Token.spacing.extra_large * 2
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            pixelAligned: true

            // The cover, blurred, drifts at half speed so the page has depth while it scrolls.
            HeroBackdrop {
                id: backdrop
                width: flick.width
                height: 520
                y: flick.contentY * 0.5
                z: -1
                source: page.info.coverUrl ?? ""
            }

            ColumnLayout {
                id: contentCol
                // A comfortable reading column on wide windows; the backdrop stays full width.
                width: Math.min(flick.width - MD.Token.spacing.large * 2, 1120)
                x: Math.round((flick.width - width) / 2)
                spacing: MD.Token.spacing.large

                // ---------- Hero: poster, title, facts, actions ----------
                RowLayout {
                    id: heroRow
                    Layout.fillWidth: true
                    Layout.topMargin: 84 // clears the floating back button
                    spacing: MD.Token.spacing.extra_large

                    MD.ElevationRectangle {
                        id: posterCard
                        Layout.preferredWidth: 240
                        Layout.preferredHeight: 320
                        Layout.alignment: Qt.AlignTop
                        radius: MD.Token.shape.corner.extra_large
                        color: MD.Token.color.surface_container
                        elevation: MD.Token.elevation.level3
                        transformOrigin: Item.Center

                        GamePoster {
                            anchors.fill: parent
                            source: page.info.coverUrl ?? ""
                            awaiting: !String(page.info.coverUrl ?? "").startsWith("file:")
                            fallbackText: (page.info.title ?? "?").charAt(0)
                            cornerRadius: MD.Token.shape.corner.extra_large
                            decodeWidth: 480
                            decodeHeight: 640
                        }

                        RevealIn { delay: 0; rise: 0.9 }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: MD.Token.spacing.medium

                        // Where it comes from: a small overline above the title.
                        MD.Label {
                            Layout.fillWidth: true
                            text: (page.sourceLabel || "").toUpperCase()
                            visible: text.length > 0
                            typescale: MD.Token.typescale.label_large
                            color: MD.Token.color.primary
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            RevealIn { delay: 40 }
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: page.info.title ?? ""
                            typescale: MD.Token.typescale.headline_large
                            wrapMode: Text.WordWrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                            RevealIn { delay: 80 }
                        }

                        // Genres as one scrolling strip (Steam dumps dozens of tags).
                        Flickable {
                            id: genreStrip
                            Layout.fillWidth: true
                            Layout.preferredHeight: genreRow.implicitHeight
                            visible: genreTokens.length > 0
                            clip: true
                            contentWidth: genreRow.implicitWidth
                            contentHeight: height
                            flickableDirection: Flickable.HorizontalFlick
                            boundsBehavior: Flickable.StopAtBounds
                            interactive: contentWidth > width

                            property bool expanded: false
                            readonly property int collapsedCount: 5
                            readonly property var genreTokens: {
                                const raw = (page.info.genres ?? "").toString().split(",")
                                const out = []
                                for (let i = 0; i < raw.length; ++i) {
                                    const t = raw[i].trim()
                                    // DRM has its own badge below.
                                    if (t.length && t.toLowerCase() !== "drm")
                                        out.push(t)
                                }
                                return out
                            }

                            Row {
                                id: genreRow
                                spacing: MD.Token.spacing.extra_small

                                Repeater {
                                    model: genreStrip.expanded ? genreStrip.genreTokens
                                                               : genreStrip.genreTokens.slice(0, genreStrip.collapsedCount)

                                    DetailsChip {
                                        required property var modelData
                                        text: modelData
                                    }
                                }

                                DetailsChip {
                                    visible: genreStrip.genreTokens.length > genreStrip.collapsedCount
                                    text: genreStrip.expanded
                                          ? qsTr("Less")
                                          : "+" + (genreStrip.genreTokens.length - genreStrip.collapsedCount)
                                    onClicked: genreStrip.expanded = !genreStrip.expanded
                                }
                            }

                            RevealIn { target: genreStrip; delay: 120 }
                        }

                        // Facts as tiles: easier to scan than a row of chips.
                        GridLayout {
                            id: factGrid
                            Layout.fillWidth: true
                            columns: width >= 640 ? 4 : 2
                            columnSpacing: MD.Token.spacing.small
                            rowSpacing: MD.Token.spacing.small

                            DetailsStatTile {
                                Layout.fillWidth: true
                                iconName: MD.Token.icon.storefront
                                label: qsTr("Source")
                                value: page.sourceLabel
                                RevealIn { delay: 160 }
                            }
                            DetailsStatTile {
                                Layout.fillWidth: true
                                iconName: MD.Token.icon.tag
                                label: qsTr("Version")
                                value: "v" + (page.info.version ?? "")
                                tip: qsTr("Build number of the game files. A higher number means a newer build.")
                                RevealIn { delay: 200 }
                            }
                            DetailsStatTile {
                                Layout.fillWidth: true
                                visible: !!(page.info.sizeLabel)
                                iconName: MD.Token.icon.hard_drive
                                label: qsTr("Size")
                                value: page.info.sizeLabel ?? ""
                                RevealIn { delay: 240 }
                            }
                            DetailsStatTile {
                                Layout.fillWidth: true
                                visible: (page.info.installKindLabel ?? "").length > 0
                                iconName: MD.Token.icon.install_desktop
                                label: qsTr("Install")
                                value: page.info.installKindLabel ?? ""
                                accent: MD.Token.color.secondary_container
                                onAccent: MD.Token.color.on_secondary_container
                                tip: {
                                    switch (page.info.installKind) {
                                        case 0: return qsTr("Portable: Arachnel unpacks the game into your library folder. No installer to run.")
                                        case 1: return qsTr("Installer: after the download you click Install and follow the game's own setup.")
                                        case 2: return qsTr("Bundled fix: the files needed to run the game are already included in the download.")
                                        case 3: return qsTr("Separate fix: the game and the files needed to run it are downloaded separately.")
                                    }
                                    return ""
                                }
                                RevealIn { delay: 280 }
                            }
                        }

                        // Status badges: only the ones that apply.
                        Flow {
                            id: badgeFlow
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small
                            visible: !!(page.info.hasDrm) || addonsVisible || !!(page.info.hasWorkshop)
                                     || trustVisible || !!(page.info.hasUpdate)

                            readonly property bool addonsVisible: !!(page.info.hasAddons)
                                || ((page.info.installedComponentCount ?? 0) > 0)
                                || ((page.info.componentCount ?? 0) > 0)
                            readonly property bool trustVisible: page.installSourceCount <= 1
                                && (page.info.sourceId ?? "") === "steamidra"

                            DetailsChip {
                                id: drmBadge
                                visible: !!(page.info.hasDrm)
                                text: qsTr("DRM")
                                icon.name: MD.Token.icon.shield
                                mdState.backgroundColor: MD.Token.color.error_container
                                mdState.textColor: MD.Token.color.on_error_container
                                mdState.iconColor: MD.Token.color.on_error_container
                                mdState.outlineColor: MD.Token.color.error_container
                            }
                            DetailsChip {
                                id: addonsBadge
                                visible: badgeFlow.addonsVisible
                                text: {
                                    const total = page.info.componentCount ?? 0
                                    if (page.playable && total > 0)
                                        return qsTr("%n add-ons", "", total)
                                    return qsTr("%n add-ons", "", page.info.addonCount ?? total)
                                }
                                icon.name: MD.Token.icon.extension
                                onClicked: {
                                    if (page.playable)
                                        gameSettingsSheet.openForGame(page.gameId)
                                }
                            }
                            DetailsChip {
                                id: workshopBadge
                                visible: !!(page.info.hasWorkshop)
                                text: qsTr("Workshop")
                                icon.name: MD.Token.icon.handyman
                            }
                            DetailsChip {
                                id: trustBadge
                                visible: badgeFlow.trustVisible
                                text: qsTr("Steam CDN · Online Fix")
                                icon.name: MD.Token.icon.check_circle
                                mdState.backgroundColor: MD.Token.color.tertiary_container
                                mdState.textColor: MD.Token.color.on_tertiary_container
                                mdState.iconColor: MD.Token.color.on_tertiary_container
                                mdState.outlineColor: MD.Token.color.tertiary_container
                                onClicked: page.openSteamidraTrust()
                            }
                            DetailsChip {
                                id: updateBadge
                                visible: !!(page.info.hasUpdate)
                                text: qsTr("Update available")
                                icon.name: MD.Token.icon.update
                                mdState.backgroundColor: MD.Token.color.tertiary_container
                                mdState.textColor: MD.Token.color.on_tertiary_container
                                mdState.iconColor: MD.Token.color.on_tertiary_container
                                mdState.outlineColor: MD.Token.color.tertiary_container
                            }
                            RevealIn { delay: 320 }
                        }
                    }
                }

                // ---------- Action dock ----------
                MD.ElevationRectangle {
                    id: actionDock
                    Layout.fillWidth: true
                    Layout.preferredHeight: dockCol.implicitHeight + 2 * MD.Token.spacing.medium
                    radius: MD.Token.shape.corner.extra_large
                    color: MD.Token.color.surface_container_high
                    elevation: MD.Token.elevation.level1
                    transformOrigin: Item.Top

                    ColumnLayout {
                        id: dockCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: MD.Token.spacing.medium
                        spacing: MD.Token.spacing.small

                        // Messages that explain the state the buttons are in.
                        MD.Label {
                            Layout.fillWidth: true
                            visible: !page.playable
                            text: Messages.gamePlayabilityNote
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: page.readyToInstall && !page.installFailed
                                     && (page.info.sourceId ?? "") !== "steamidra"
                            text: Messages.gameInstallTorrentHint
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: page.readyToInstall && !page.installFailed
                                     && (page.info.sourceId ?? "") === "steamidra"
                            text: qsTr("Ready to download from Steam CDN. Online Fix can be included when needed.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: page.downloadFailed || page.installFailed
                            text: page.downloadJob.detail
                                  || (page.downloadFailed ? qsTr("Download failed")
                                                          : qsTr("Install failed"))
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.error
                            typescale: MD.Token.typescale.body_medium
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: page.canManualInstall
                            text: qsTr("Couldn't install automatically. Run the installer, then pick the folder with the folder button.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.medium

                            Flow {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.small

                                MD.Button {
                                    visible: page.playable
                                    enabled: !page.runtimeSetupActive
                                    text: {
                                        if (page.isRunning)
                                            return qsTr("Stop")
                                        if (page.runtimeSetupActive)
                                            return Core.runtimeSetupStatus.length > 0
                                                   ? Core.runtimeSetupStatus
                                                   : qsTr("Preparing…")
                                        return qsTr("Play")
                                    }
                                    icon.name: page.isRunning || page.runtimeSetupActive
                                             ? "" : MD.Token.icon.play_arrow
                                    mdState.type: MD.Enum.BtFilled
                                    mdState.backgroundColor: page.isRunning
                                                         ? MD.Token.color.error
                                                         : MD.Token.color.primary
                                    mdState.textColor: page.isRunning
                                                       ? MD.Token.color.on_error
                                                       : MD.Token.color.on_primary
                                    onClicked: page.isRunning
                                                 ? Core.stopRunningGame()
                                                 : Core.launchGame(page.gameId)
                                }

                                DownloadProgressButton {
                                    id: downloadAction
                                    visible: page.canManageDownload
                                    embedDetail: false
                                    progress: page.downloadJob.progress ?? 0
                                    bytesDownloaded: Number(page.effectiveDownloaded) || 0
                                    totalBytes: Number(page.downloadTotalBytes) || 0
                                    detail: page.downloadJob.detail ?? ""
                                    downloading: page.downloadActive
                                    paused: page.downloadPaused
                                    completed: false
                                    readyToInstall: page.readyToInstall
                                    downloadFailed: page.downloadFailed
                                    installFailed: page.installFailed
                                    installing: page.isInstalling
                                    onActivated: {
                                        // Prefer retryInstall when files are already on disk.
                                        if (page.installFailed || page.readyToInstall)
                                            Core.retryInstall(page.downloadJob.jobId)
                                        else if (page.downloadFailed)
                                            Core.retryJob(page.downloadJob.jobId)
                                        else
                                            installStudio.openStudio()
                                    }
                                    onPauseToggleRequested: Core.toggleJobPause(page.downloadJob.jobId)
                                    onCancelRequested: Core.cancelJob(page.downloadJob.jobId)
                                }

                                MD.Button {
                                    visible: page.installed && !!(page.info.hasUpdate) && !page.downloadJob.inProgress
                                    text: qsTr("Update")
                                    icon.name: MD.Token.icon.update
                                    mdState.type: MD.Enum.BtFilledTonal
                                    onClicked: {
                                        if (Core.catalogUpdateHasDlcRisk(page.gameId))
                                            dlcUpdateRiskDialog.openForGame(page.gameId)
                                        else
                                            Core.updateCatalogEntry(page.gameId)
                                    }
                                }
                            }

                            // Secondary actions live in one rounded rail.
                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                implicitWidth: railRow.implicitWidth + MD.Token.spacing.small * 2
                                implicitHeight: railRow.implicitHeight + MD.Token.spacing.extra_small * 2
                                radius: MD.Token.shape.corner.full
                                color: MD.Token.color.surface_container_highest

                                Row {
                                    id: railRow
                                    anchors.centerIn: parent
                                    spacing: 0

                                    MD.IconButton {
                                        visible: page.canManualInstall
                                        mdState.type: MD.Enum.IBtStandard
                                        icon.name: MD.Token.icon.folder_open
                                        onClicked: Core.confirmManualInstall(page.downloadJob.jobId)

                                        MD.ToolTip {
                                            visible: parent.hovered
                                            text: qsTr("Pick the folder you installed into.")
                                        }
                                    }

                                    MD.IconButton {
                                        readonly property bool favorited: {
                                            const ids = Core.settings.bookmarkedEntryIds
                                            return ids.indexOf(page.gameId) >= 0
                                        }
                                        checked: favorited
                                        mdState.type: MD.Enum.IBtStandard
                                        icon.name: MD.Token.icon.favorite
                                        Accessible.name: favorited
                                                         ? qsTr("Remove from favorites")
                                                         : qsTr("Add to favorites")
                                        onClicked: Core.toggleBookmark(page.gameId)
                                    }

                                    MD.IconButton {
                                        visible: hasLaunchLog
                                        mdState.type: MD.Enum.IBtStandard
                                        icon.name: MD.Token.icon.receipt_long
                                        Accessible.name: qsTr("Launch log")
                                        onClicked: openLaunchLog()
                                    }

                                    MD.IconButton {
                                        visible: page.playable
                                                 || page.downloadComplete
                                                 || page.inLibrary
                                        mdState.type: MD.Enum.IBtStandard
                                        icon.name: MD.Token.icon.settings
                                        Accessible.name: qsTr("Game settings")
                                        onClicked: gameSettingsSheet.openForGame(page.gameId)
                                    }

                                    MD.IconButton {
                                        id: moreActions
                                        visible: page.playable
                                                 || page.downloadComplete
                                                 || page.inLibrary
                                        mdState.type: MD.Enum.IBtStandard
                                        icon.name: MD.Token.icon.more_vert
                                        Accessible.name: qsTr("More actions")
                                        onClicked: moreMenu.open()

                                        MD.Menu {
                                            id: moreMenu
                                            y: parent.height
                                            autoClose: true

                                            MD.MenuItem {
                                                text: qsTr("Delete")
                                                icon.name: MD.Token.icon.delete
                                                onTriggered: removeDialog.open()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: downloadAction.visible && downloadAction.transferDetailVisible
                            text: downloadAction.transferLine
                            color: MD.Token.color.primary
                            typescale: MD.Token.typescale.label_large
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }
                    }

                    RevealIn { delay: 360 }
                }

                // ---------- Install studio: grows out of the dock ----------
                InstallStudio {
                    id: installStudio
                    Layout.fillWidth: true
                    page: detailsRoot.page
                    // While the card grows, keep its top just under the page header.
                    onImplicitHeightChanged: {
                        if (!open || reveal >= 1)
                            return
                        const wanted = installStudio.mapToItem(flick.contentItem, 0, 0).y - 96
                        const limit = flick.contentHeight - flick.height
                        if (flick.contentY < wanted && !scrollAnim.running)
                            flick.contentY = Math.max(0, Math.min(wanted, limit))
                    }
                }

                // ---------- Description ----------
                DetailsSection {
                    id: aboutCard
                    Layout.fillWidth: true
                    title: qsTr("Description")
                    iconName: MD.Token.icon.description
                    transformOrigin: Item.Top

                    property bool expanded: false
                    // Four lines until the reader asks for more; the height eases between the two.
                    readonly property real collapsedHeight: aboutText.lineCount > 0
                        ? aboutText.implicitHeight / aboutText.lineCount * 4 : 96
                    readonly property bool canExpand: aboutText.lineCount > 4
                    property real shownHeight: expanded || !canExpand ? aboutText.implicitHeight
                                                                      : collapsedHeight

                    Behavior on shownHeight {
                        NumberAnimation {
                            duration: AppMotion.medium
                            easing: AppMotion.emphasizedDecelerate
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: aboutCard.shownHeight
                        clip: true

                        MD.Label {
                            id: aboutText
                            width: parent.width
                            text: page.info.description || qsTr("Description is not available yet.")
                            typescale: MD.Token.typescale.body_large
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                        }
                    }

                    MD.Button {
                        visible: aboutCard.canExpand
                        text: aboutCard.expanded ? qsTr("Show less") : qsTr("Show more")
                        icon.name: aboutCard.expanded ? MD.Token.icon.expand_less : MD.Token.icon.expand_more
                        mdState.type: MD.Enum.BtText
                        onClicked: aboutCard.expanded = !aboutCard.expanded
                    }

                    RevealIn { target: aboutCard; flickable: flick; delay: 60 }
                }

                // ---------- Media ----------
                DetailsSection {
                    id: mediaCard
                    Layout.fillWidth: true
                    visible: mediaSection.showSection
                    transformOrigin: Item.Top

                    GameDetailsMediaSection {
                        id: mediaSection
                        Layout.fillWidth: true
                        screenshotUrls: page.info.screenshotUrls ?? []
                        trailerUrl: page.info.trailerUrl ?? ""
                        trailerThumbnailUrl: page.info.trailerThumbnailUrl ?? ""
                        loading: page.mediaLoading
                    }

                    RevealIn { target: mediaCard; flickable: flick; delay: 60 }
                }

                // ---------- Links ----------
                DetailsSection {
                    id: linksCard
                    Layout.fillWidth: true
                    title: qsTr("Links")
                    iconName: MD.Token.icon.link
                    transformOrigin: Item.Top
                    visible: (page.gameId || "").length > 0
                             || (page.info.sourcePageUrl ?? "").length > 0
                             || (page.info.sourceWebsiteUrl ?? "").length > 0
                             || (page.info.steamStoreUrl ?? "").length > 0
                             || (Core.sources.repositoryUrlFor(page.info.sourceId ?? "") || "").length > 0
                             || (Core.sources.catalogUrlFor(page.info.sourceId ?? "") || "").length > 0

                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            id: shareButton
                            visible: (page.gameId || "").length > 0
                            text: qsTr("Share")
                            icon.name: MD.Token.icon.share
                            mdState.type: MD.Enum.BtFilledTonal
                            onClicked: shareDialog.open()
                        }

                        MD.Button {
                            id: sourceButton
                            visible: (page.info.sourcePageUrl ?? "").length > 0
                                     || (page.info.sourceWebsiteUrl ?? "").length > 0
                            text: (page.info.sourcePageUrl ?? "").length > 0
                                  ? qsTr("Source page")
                                  : qsTr("Source website")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: Core.openExternalUrl(
                                (page.info.sourcePageUrl ?? "").length > 0
                                    ? page.info.sourcePageUrl
                                    : page.info.sourceWebsiteUrl)
                        }

                        MD.Button {
                            id: steamButton
                            visible: (page.info.steamStoreUrl ?? "").length > 0
                            text: qsTr("Steam")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: Core.openExternalUrl(page.info.steamStoreUrl)
                        }

                        MD.Button {
                            id: pluginButton
                            visible: (Core.sources.repositoryUrlFor(page.info.sourceId ?? "") || "").length > 0
                            text: qsTr("Plugin source")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: Core.openExternalUrl(
                                Core.sources.repositoryUrlFor(page.info.sourceId ?? ""))
                        }

                        MD.Button {
                            id: catalogButton
                            visible: (Core.sources.catalogUrlFor(page.info.sourceId ?? "") || "").length > 0
                            text: qsTr("Catalog URL")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: Core.openExternalUrl(
                                Core.sources.catalogUrlFor(page.info.sourceId ?? ""))
                        }
                    }

                    RevealIn { target: linksCard; flickable: flick; delay: 60 }
                }
            }
        }
    }

    // Once the hero has scrolled away a slim bar keeps the title and the favorite toggle in reach.
    Rectangle {
        id: compactBar
        readonly property bool shown: page.gameFound && flick.contentY > 280
        anchors.left: parent.left
        anchors.right: parent.right
        y: shown ? 0 : -height
        height: 64
        z: 9
        color: MD.Token.color.surface_container
        opacity: shown ? 1 : 0
        visible: opacity > 0

        Behavior on y {
            NumberAnimation {
                duration: AppMotion.medium
                easing: AppMotion.emphasizedDecelerate
            }
        }
        Behavior on opacity { NumberAnimation { duration: AppMotion.short } }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: MD.Token.color.outline_variant
            opacity: 0.5
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: MD.Token.spacing.large * 2 + 40
            anchors.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.medium

            MD.Label {
                Layout.fillWidth: true
                text: page.info.title ?? ""
                typescale: MD.Token.typescale.title_medium
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            // Jump to a section with one click; the current one follows the scroll.
            Row {
                id: sectionNav
                visible: compactBar.width > 760
                spacing: MD.Token.spacing.extra_small

                readonly property real y0: flick.contentY + flick.height * 0.4
                readonly property bool atEnd: flick.contentY >= flick.contentHeight - flick.height - 4
                                              && flick.contentHeight > flick.height
                readonly property int current: {
                    if (linksCard.visible && (y0 >= linksCard.y || atEnd))
                        return 3
                    if (mediaCard.visible && y0 >= mediaCard.y)
                        return 2
                    if (y0 >= aboutCard.y)
                        return 1
                    return 0
                }

                Repeater {
                    model: [
                        { label: qsTr("Overview"), target: 0 },
                        { label: qsTr("Description"), target: 1 },
                        { label: qsTr("Media"), target: 2 },
                        { label: qsTr("Links"), target: 3 }
                    ]

                    delegate: MD.Button {
                        required property var modelData
                        visible: modelData.target !== 2 || mediaCard.visible
                        text: modelData.label
                        mdState.type: sectionNav.current === modelData.target
                                      ? MD.Enum.BtFilledTonal : MD.Enum.BtText
                        onClicked: {
                            if (modelData.target === 0)
                                detailsRoot.scrollToTop()
                            else if (modelData.target === 1)
                                detailsRoot.scrollToItem(aboutCard, 84)
                            else if (modelData.target === 2)
                                detailsRoot.scrollToItem(mediaCard, 84)
                            else
                                detailsRoot.scrollToItem(linksCard, 84)
                        }
                    }
                }
            }

            MD.IconButton {
                readonly property bool favorited: {
                    const ids = Core.settings.bookmarkedEntryIds
                    return ids.indexOf(page.gameId) >= 0
                }
                checked: favorited
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.favorite
                Accessible.name: favorited ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                onClicked: Core.toggleBookmark(page.gameId)
            }

            MD.IconButton {
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.keyboard_arrow_up
                Accessible.name: qsTr("Back to top")
                onClicked: detailsRoot.scrollToTop()
            }
        }
    }

    // Floats over the hero instead of a dedicated "Game details" title bar.
    MD.IconButton {
        id: backButton
        x: MD.Token.spacing.large
        y: MD.Token.spacing.large
        z: 10
        mdState.type: MD.Enum.IBtFilledTonal
        icon.name: MD.Token.icon.arrow_back
        Accessible.name: qsTr("Back")
        onClicked: page.backRequested()
    }

    GameSettingsSheet {
        id: gameSettingsSheet
        anchors.fill: parent
    }

    LaunchLogDialog {
        id: launchLogDialog
    }

    MD.Dialog {
        id: shareDialog
        title: qsTr("Share")
        modal: true
        width: Math.min(420, page.width > 0 ? page.width - 48 : 420)

        property var selectedFriendIds: []

        function toggleFriend(friendId) {
            const next = selectedFriendIds.slice()
            const i = next.indexOf(friendId)
            if (i >= 0)
            next.splice(i, 1)
            else
            next.push(friendId)
            selectedFriendIds = next
        }

        function sendSuggestions() {
            if (selectedFriendIds.length === 0)
            return
            Core.suggestGameToFriends(selectedFriendIds, page.gameId)
            close()
        }

        onAboutToShow: selectedFriendIds = []

        ColumnLayout {
            width: shareDialog.width - shareDialog.horizontalPadding * 2
            spacing: MD.Token.spacing.medium

            MD.Button {
                Layout.fillWidth: true
                text: qsTr("Copy link")
                icon.name: MD.Token.icon.link
                mdState.type: MD.Enum.BtFilledTonal
                onClicked: {
                    Core.shareGameLink(page.gameId)
                    shareDialog.close()
                }
            }

            MD.Label {
                Layout.fillWidth: true
                visible: Core.social.friends.count > 0
                text: qsTr("Suggest to friends")
                typescale: MD.Token.typescale.title_small
            }

            Flow {
                Layout.fillWidth: true
                visible: Core.social.friends.count > 0
                spacing: MD.Token.spacing.small

                Repeater {
                    model: Core.social.friends

                    Column {
                        id: friendChip
                        required property string friendId
                        required property string nickname
                        required property bool online

                        readonly property bool selected: shareDialog.selectedFriendIds.indexOf(friendId) >= 0
                        readonly property real avatarSize: MD.Token.spacing.extra_large
                                                           + MD.Token.spacing.small
                        readonly property real ringPad: 3
                        readonly property real ringWidth: 2
                        readonly property real chipWidth: avatarSize + (ringPad + ringWidth) * 2

                        width: chipWidth
                        spacing: MD.Token.spacing.extra_small

                        Item {
                            id: avatarArea
                            width: friendChip.chipWidth
                            height: width

                            // Border-only fill must match dialog; transparent + border paints black.
                            Rectangle {
                                anchors.fill: parent
                                visible: friendChip.online || friendChip.selected
                                radius: width / 2
                                color: MD.Token.color.surface_container_high
                                border.width: friendChip.ringWidth
                                border.color: MD.Token.color.primary
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                width: friendChip.avatarSize
                                height: friendChip.avatarSize
                                radius: width / 2
                                color: friendChip.selected
                                       ? MD.Token.color.primary_container
                                       : MD.Token.color.surface_container_highest

                                MD.Label {
                                    anchors.centerIn: parent
                                    text: friendChip.nickname.length
                                          ? friendChip.nickname.charAt(0).toUpperCase()
                                          : "?"
                                    typescale: MD.Token.typescale.title_small
                                    color: friendChip.selected
                                           ? MD.Token.color.on_primary_container
                                           : MD.Token.color.on_surface
                                }
                            }

                            Rectangle {
                                visible: friendChip.selected
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                width: MD.Token.spacing.medium + 2
                                height: width
                                radius: width / 2
                                color: MD.Token.color.primary

                                MD.Icon {
                                    anchors.centerIn: parent
                                    name: MD.Token.icon.check
                                    size: MD.Token.spacing.medium - 2
                                    color: MD.Token.color.on_primary
                                }
                            }
                        }

                        Item {
                            id: nameClip
                            width: friendChip.chipWidth
                            height: nameLabel.implicitHeight
                            clip: true

                            readonly property real overflow: Math.max(0, nameLabel.implicitWidth - width)
                            readonly property bool needsScroll: overflow > 1

                            MD.Label {
                                id: nameLabel
                                y: 0
                                text: friendChip.nickname
                                typescale: MD.Token.typescale.label_small
                                color: MD.Token.color.on_surface_variant
                                maximumLineCount: 1
                            }

                            Binding {
                                target: nameLabel
                                property: "x"
                                value: Math.max(0, (nameClip.width - nameLabel.implicitWidth) / 2)
                                when: !nameClip.needsScroll
                                restoreMode: Binding.RestoreNone
                            }

                            SequentialAnimation {
                                running: nameClip.needsScroll && shareDialog.visible
                                loops: Animation.Infinite

                                PauseAnimation {
                                    duration: 900
                                }
                                NumberAnimation {
                                    target: nameLabel
                                    property: "x"
                                    from: 0
                                    to: -nameClip.overflow
                                    duration: Math.max(1200, nameClip.overflow * 35)
                                    easing.type: Easing.InOutSine
                                }
                                PauseAnimation {
                                    duration: 700
                                }
                                NumberAnimation {
                                    target: nameLabel
                                    property: "x"
                                    from: -nameClip.overflow
                                    to: 0
                                    duration: Math.max(1200, nameClip.overflow * 35)
                                    easing.type: Easing.InOutSine
                                }
                            }
                        }

                        HoverHandler {
                            cursorShape: Qt.PointingHandCursor
                        }

                        TapHandler {
                            onTapped: shareDialog.toggleFriend(friendChip.friendId)
                        }
                    }
                }
            }
        }

        footer: Item {
            implicitHeight: shareFooterRow.implicitHeight + MD.Token.spacing.medium

            MD.DialogButtonBox {
                id: shareFooterRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top

                MD.Button {
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Close")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                    onClicked: shareDialog.close()
                }

                MD.Button {
                    visible: Core.social.friends.count > 0
                    enabled: shareDialog.selectedFriendIds.length > 0
                    mdState.type: MD.Enum.BtFilled
                    text: shareDialog.selectedFriendIds.length > 1
                          ? qsTr("Send (%1)").arg(shareDialog.selectedFriendIds.length)
                          : qsTr("Send")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    onClicked: shareDialog.sendSuggestions()
                }
            }
        }
    }

    MD.Dialog {
        id: removeDialog
        title: qsTr("Remove game?")
        modal: true
        width: Math.min(420, page.width > 0 ? page.width - 48 : 420)

        MD.Label {
            width: removeDialog.width - removeDialog.horizontalPadding * 2
            text: Messages.gameDeleteWarning
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        footer: Item {
            implicitHeight: removeFooterRow.implicitHeight + MD.Token.spacing.medium

            MD.DialogButtonBox {
                id: removeFooterRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top

                MD.Button {
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Cancel")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                    onClicked: removeDialog.close()
                }
                MD.Button {
                    mdState.type: MD.Enum.BtFilled
                    text: qsTr("Delete")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    onClicked: {
                        removeDialog.close()
                        page.confirmRemove()
                    }
                }
            }
        }
    }

    DlcUpdateRiskDialog {
        id: dlcUpdateRiskDialog
        width: Math.min(420, page.width > 0 ? page.width - 48 : 420)
        onUpdateAccepted: function(gameId) {
            Core.updateCatalogEntry(gameId)
        }
    }
}
