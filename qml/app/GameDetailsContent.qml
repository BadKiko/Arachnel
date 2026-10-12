import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: detailsRoot
    required property var page

    readonly property bool hasLaunchLog: {
        const _rev = page.detailsRevision
        return Core.hasGameLaunchLog(page.gameId)
    }

    // ---------- The page takes its accent from the poster ----------
    property color accent: MD.Token.color.primary
    readonly property color accentContainer: Qt.rgba(accent.r, accent.g, accent.b, 0.20)
    readonly property color accentInk: "#121216"

    Behavior on accent {
        ColorAnimation { duration: AppMotion.scaled(600) }
    }

    readonly property string coverForAccent: String(page.info.coverUrl ?? "")

    // Averages the saturated pixels of a tiny copy of the poster; lightness is clamped so the
    // colour reads on the dark page and dark text reads on it.
    Canvas {
        id: accentSampler
        x: -64
        width: 24
        height: 24
        opacity: 0.01
        renderTarget: Canvas.Image
        property string src: detailsRoot.coverForAccent

        function load() {
            if (src.startsWith("file:"))
                loadImage(src)
        }
        onSrcChanged: load()
        Component.onCompleted: load()
        onImageLoaded: requestPaint()
        onPaint: {
            if (!src.startsWith("file:") || !isImageLoaded(src))
                return
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.drawImage(src, 0, 0, width, height)
            const d = ctx.getImageData(0, 0, width, height).data
            let r = 0, g = 0, b = 0, w = 0
            for (let i = 0; i < d.length; i += 4) {
                const mx = Math.max(d[i], d[i + 1], d[i + 2])
                const mn = Math.min(d[i], d[i + 1], d[i + 2])
                const sat = mx === 0 ? 0 : (mx - mn) / mx
                const weight = sat * sat * (mx / 255) + 0.001
                r += d[i] * weight
                g += d[i + 1] * weight
                b += d[i + 2] * weight
                w += weight
            }
            if (w <= 0)
                return
            const c = Qt.rgba(r / w / 255, g / w / 255, b / w / 255, 1)
            detailsRoot.accent = Qt.hsla(Math.max(0, c.hslHue),
                                         Math.max(0.45, Math.min(0.9, c.hslSaturation)),
                                         Math.max(0.64, Math.min(0.76, c.hslLightness + 0.15)), 1)
        }
    }

    // Theme font family for the large title (a plain Text can take any pixel size).
    MD.Label {
        id: fontRef
        visible: false
        typescale: MD.Token.typescale.headline_large
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
                height: 560
                y: flick.contentY * 0.5
                z: -1
                source: page.info.coverUrl ?? ""
            }

            // A wash of the poster's colour over the top of the page.
            Rectangle {
                width: flick.width
                height: 560
                y: flick.contentY * 0.5
                z: -1
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.rgba(detailsRoot.accent.r, detailsRoot.accent.g, detailsRoot.accent.b, 0.22) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            ColumnLayout {
                id: contentCol
                width: Math.min(flick.width - MD.Token.spacing.large * 2, 1120)
                x: Math.round((flick.width - width) / 2)
                spacing: MD.Token.spacing.large

                // ================= Hero =================
                RowLayout {
                    id: heroRow
                    Layout.fillWidth: true
                    Layout.topMargin: 84 // clears the floating back button
                    spacing: MD.Token.spacing.extra_large

                    Item {
                        id: posterBox
                        Layout.preferredWidth: 248
                        Layout.preferredHeight: 330
                        Layout.alignment: Qt.AlignTop
                        transformOrigin: Item.Center

                        // A soft glow in the poster's own colour.
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width * 0.92
                            height: parent.height * 0.92
                            radius: 40
                            color: detailsRoot.accent
                            opacity: 0.38
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                blurEnabled: true
                                blur: 1.0
                                blurMax: 56
                            }
                        }

                        GamePoster {
                            anchors.fill: parent
                            source: page.info.coverUrl ?? ""
                            awaiting: !String(page.info.coverUrl ?? "").startsWith("file:")
                            fallbackText: (page.info.title ?? "?").charAt(0)
                            cornerRadius: 28
                            decodeWidth: 500
                            decodeHeight: 660
                        }

                        RevealIn { delay: 0; rise: 0.88 }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: MD.Token.spacing.medium

                        // Source · install kind, in the poster's colour.
                        MD.Label {
                            Layout.fillWidth: true
                            text: {
                                const parts = []
                                if ((page.sourceLabel || "").length)
                                    parts.push(page.sourceLabel)
                                if ((page.info.installKindLabel ?? "").length)
                                    parts.push(page.info.installKindLabel)
                                return parts.join("  ·  ").toUpperCase()
                            }
                            visible: text.length > 0
                            typescale: MD.Token.typescale.label_large
                            font.letterSpacing: 1.6
                            color: detailsRoot.accent
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            RevealIn { delay: 40 }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: page.info.title ?? ""
                            color: MD.Token.color.on_surface
                            font.family: fontRef.font.family
                            font.pixelSize: text.length > 34 ? 34 : 44
                            font.weight: Font.DemiBold
                            font.letterSpacing: -0.5
                            wrapMode: Text.WordWrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                            RevealIn { delay: 80 }
                        }

                        // Facts as a single quiet line of icon + value pairs.
                        Flow {
                            id: factFlow
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.large

                            Repeater {
                                model: {
                                    const out = []
                                    if ((page.info.sizeLabel ?? "").length)
                                        out.push({ icon: MD.Token.icon.hard_drive, text: page.info.sizeLabel, tip: "" })
                                    if ((page.info.version ?? "").length)
                                        out.push({ icon: MD.Token.icon.tag, text: "v" + page.info.version,
                                                   tip: qsTr("Build number of the game files. A higher number means a newer build.") })
                                    const genres = (page.info.genres ?? "").toString().split(",")
                                        .map(t => t.trim()).filter(t => t.length && t.toLowerCase() !== "drm")
                                    if (genres.length)
                                        out.push({ icon: MD.Token.icon.sports_esports,
                                                   text: genres.slice(0, 3).join(", ") + (genres.length > 3 ? "…" : ""),
                                                   tip: genres.join(", ") })
                                    return out
                                }

                                delegate: Row {
                                    id: fact
                                    required property var modelData
                                    spacing: 6

                                    MD.Icon {
                                        anchors.verticalCenter: parent.verticalCenter
                                        name: fact.modelData.icon
                                        size: 18
                                        color: MD.Token.color.on_surface_variant
                                    }
                                    MD.Label {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: fact.modelData.text
                                        typescale: MD.Token.typescale.title_small
                                        color: MD.Token.color.on_surface_variant

                                        HoverHandler { id: factHover }
                                        MD.ToolTip {
                                            visible: factHover.hovered && fact.modelData.tip.length > 0
                                            text: fact.modelData.tip
                                        }
                                    }
                                }
                            }
                            RevealIn { delay: 120 }
                        }

                        // Status badges, only the ones that apply.
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
                                visible: !!(page.info.hasDrm)
                                text: qsTr("DRM")
                                icon.name: MD.Token.icon.shield
                                mdState.backgroundColor: MD.Token.color.error_container
                                mdState.textColor: MD.Token.color.on_error_container
                                mdState.iconColor: MD.Token.color.on_error_container
                                mdState.outlineColor: MD.Token.color.error_container
                            }
                            DetailsChip {
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
                                visible: !!(page.info.hasWorkshop)
                                text: qsTr("Workshop")
                                icon.name: MD.Token.icon.handyman
                            }
                            DetailsChip {
                                visible: badgeFlow.trustVisible
                                text: qsTr("Steam CDN · Online Fix")
                                icon.name: MD.Token.icon.check_circle
                                onClicked: page.openSteamidraTrust()
                            }
                            DetailsChip {
                                visible: !!(page.info.hasUpdate)
                                text: qsTr("Update available")
                                icon.name: MD.Token.icon.update
                                // Solid accent like the Play button: the old 20% tint with grey text
                                // read as a disabled chip.
                                mdState.backgroundColor: detailsRoot.accent
                                mdState.textColor: detailsRoot.accentInk
                                mdState.iconColor: detailsRoot.accentInk
                                mdState.outlineColor: detailsRoot.accent
                            }
                            RevealIn { delay: 160 }
                        }

                        // ---------- Actions: right in the hero, no extra card ----------
                        RowLayout {
                            id: actionRow
                            Layout.fillWidth: true
                            Layout.topMargin: MD.Token.spacing.small
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
                                mdState.size: MD.Enum.M
                                mdState.backgroundColor: page.isRunning ? MD.Token.color.error : detailsRoot.accent
                                mdState.textColor: page.isRunning ? MD.Token.color.on_error : detailsRoot.accentInk
                                onClicked: page.isRunning ? Core.stopRunningGame() : Core.launchGame(page.gameId)
                            }

                            DownloadProgressButton {
                                id: downloadAction
                                visible: page.canManageDownload && !installStudio.open
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

                            MD.IconButton {
                                visible: page.canManualInstall
                                mdState.type: MD.Enum.IBtFilledTonal
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
                                mdState.type: MD.Enum.IBtFilledTonal
                                icon.name: MD.Token.icon.favorite
                                Accessible.name: favorited ? qsTr("Remove from favorites") : qsTr("Add to favorites")
                                onClicked: Core.toggleBookmark(page.gameId)
                            }

                            MD.IconButton {
                                visible: hasLaunchLog
                                mdState.type: MD.Enum.IBtFilledTonal
                                icon.name: MD.Token.icon.receipt_long
                                Accessible.name: qsTr("Launch log")
                                onClicked: openLaunchLog()
                            }

                            MD.IconButton {
                                visible: page.playable || page.downloadComplete || page.inLibrary
                                mdState.type: MD.Enum.IBtFilledTonal
                                icon.name: MD.Token.icon.settings
                                Accessible.name: qsTr("Game settings")
                                onClicked: gameSettingsSheet.openForGame(page.gameId)
                            }

                            MD.IconButton {
                                id: moreActions
                                visible: page.playable || page.downloadComplete || page.inLibrary
                                mdState.type: MD.Enum.IBtFilledTonal
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

                            Item { Layout.fillWidth: true }

                            RevealIn { delay: 200 }
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: downloadAction.visible && downloadAction.transferDetailVisible
                            text: downloadAction.transferLine
                            color: detailsRoot.accent
                            typescale: MD.Token.typescale.label_large
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }

                        // Notes about the state the buttons are in.
                        MD.Label {
                            Layout.fillWidth: true
                            visible: page.downloadFailed || page.installFailed
                            text: page.downloadJob.detail
                                  || (page.downloadFailed ? qsTr("Download failed") : qsTr("Install failed"))
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
                        MD.Label {
                            Layout.fillWidth: true
                            visible: page.readyToInstall && !page.installFailed
                            text: (page.info.sourceId ?? "") === "steamidra"
                                  ? qsTr("Ready to download from Steam CDN. Online Fix can be included when needed.")
                                  : Messages.gameInstallTorrentHint
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }
                        // Only worth saying when the game actually has DRM.
                        MD.Label {
                            Layout.fillWidth: true
                            visible: !page.playable && !!(page.info.hasDrm)
                            text: Messages.gamePlayabilityNote
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            opacity: 0.8
                            typescale: MD.Token.typescale.body_small
                        }
                    }
                }

                // ================= Install =================
                InstallStudio {
                    id: installStudio
                    Layout.fillWidth: true
                    Layout.topMargin: open ? MD.Token.spacing.medium : 0
                    page: detailsRoot.page
                    accent: detailsRoot.accent
                    accentContainer: detailsRoot.accentContainer
                    accentInk: detailsRoot.accentInk
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

                // ================= Description =================
                DetailsSection {
                    id: aboutCard
                    Layout.fillWidth: true
                    Layout.topMargin: MD.Token.spacing.medium
                    title: qsTr("Description")
                    iconName: MD.Token.icon.description
                    iconColor: detailsRoot.accent
                    transformOrigin: Item.Top

                    property bool expanded: false
                    // Four lines until the reader asks for more; the height eases between the two.
                    readonly property real collapsedHeight: aboutText.lineCount > 0
                        ? aboutText.implicitHeight / aboutText.lineCount * 4 : 96
                    readonly property bool canExpand: aboutText.lineCount > 4
                    property real shownHeight: expanded || !canExpand ? aboutText.implicitHeight
                                                                      : collapsedHeight

                    Behavior on shownHeight {
                        NumberAnimation { duration: AppMotion.medium; easing: AppMotion.emphasizedDecelerate }
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

                // ================= Media =================
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

                // ================= Links =================
                DetailsSection {
                    id: linksCard
                    Layout.fillWidth: true
                    title: qsTr("Links")
                    iconName: MD.Token.icon.link
                    iconColor: detailsRoot.accent
                    transformOrigin: Item.Top
                    visible: (page.gameId || "").length > 0

                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            visible: (page.gameId || "").length > 0
                            text: qsTr("Share")
                            icon.name: MD.Token.icon.share
                            mdState.type: MD.Enum.BtFilledTonal
                            onClicked: shareDialog.open()
                        }

                        MD.Button {
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
                            visible: (page.info.steamStoreUrl ?? "").length > 0
                            text: qsTr("Steam")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: Core.openExternalUrl(page.info.steamStoreUrl)
                        }

                        MD.Button {
                            visible: (Core.sources.repositoryUrlFor(page.info.sourceId ?? "") || "").length > 0
                            text: qsTr("Plugin source")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: Core.openExternalUrl(
                                Core.sources.repositoryUrlFor(page.info.sourceId ?? ""))
                        }

                        MD.Button {
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
