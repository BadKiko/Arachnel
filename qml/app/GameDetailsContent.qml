import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    required property var page

    readonly property bool hasLaunchLog: {
        const _rev = page.detailsRevision
        return Core.hasGameLaunchLog(page.gameId)
    }

    function openLaunchLog() {
        launchLogTextArea.text = Core.gameLaunchLog(page.gameId)
        launchLogDialog.open()
    }

    Connections {
        target: Core
        function onLaunchSessionEnded(gameId, elapsedMs, suppressQuickExitLog) {
            if (gameId !== page.gameId)
                return
            page.detailsRevision++
            // Quick exit usually means crash / bad launch - show the log.
            // Skip when Online Fix auto-retries without the fix.
            if (suppressQuickExitLog)
                return
            if (elapsedMs >= 0 && elapsedMs < 20000)
                openLaunchLog()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.IconButton {
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.arrow_back
                onClicked: page.backRequested()
            }

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Game details")
                typescale: MD.Token.typescale.title_large
            }
        }

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
            contentHeight: contentCol.implicitHeight + MD.Token.spacing.large
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: contentCol
                width: flick.width
                spacing: MD.Token.spacing.large

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: MD.Token.spacing.large
                    Layout.rightMargin: MD.Token.spacing.large
                    spacing: MD.Token.spacing.large

                    GamePoster {
                        Layout.preferredWidth: 220
                        Layout.preferredHeight: 293
                        Layout.alignment: Qt.AlignTop
                        source: page.info.coverUrl ?? ""
                        fallbackText: (page.info.title ?? "?").charAt(0)
                        cornerRadius: MD.Token.shape.corner.extra_large
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: MD.Token.spacing.medium

                        MD.Label {
                            Layout.fillWidth: true
                            text: page.info.title ?? ""
                            typescale: MD.Token.typescale.headline_medium
                            wrapMode: Text.WordWrap
                        }

                    // Genres/categories as a single-row chip strip (Steam dumps dozens of tags).
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

                        readonly property var genreTokens: {
                            const raw = (page.info.genres ?? "").toString().split(",")
                            const out = []
                            for (let i = 0; i < raw.length; ++i) {
                                const t = raw[i].trim()
                                // DRM has its own status chip below.
                                if (t.length && t.toLowerCase() !== "drm")
                                    out.push(t)
                            }
                            return out
                        }

                        Row {
                            id: genreRow
                            spacing: MD.Token.spacing.extra_small

                            Repeater {
                                model: genreStrip.genreTokens

                                MD.AssistChip {
                                    required property var modelData
                                    text: modelData
                                }
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.AssistChip {
                            text: page.sourceLabel
                            icon.name: MD.Token.icon.storefront
                        }
                        MD.AssistChip {
                            text: "v" + (page.info.version ?? "")
                            icon.name: MD.Token.icon.tag
                        }
                        MD.AssistChip {
                            visible: !!(page.info.sizeLabel)
                            text: page.info.sizeLabel ?? ""
                            icon.name: MD.Token.icon.hard_drive
                        }
                        MD.AssistChip {
                            visible: !!(page.info.hasDrm)
                            text: qsTr("DRM")
                            icon.name: MD.Token.icon.shield
                            elevated: true
                            mdState.backgroundColor: MD.Token.color.error_container
                            mdState.textColor: MD.Token.color.on_error_container
                            mdState.iconColor: MD.Token.color.on_error_container
                            mdState.outlineColor: MD.Token.color.error_container
                        }
                        MD.AssistChip {
                            visible: !!(page.info.hasAddons) || ((page.info.installedComponentCount ?? 0) > 0)
                                     || ((page.info.componentCount ?? 0) > 0)
                            text: {
                                const installed = page.info.installedComponentCount ?? 0
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
                        MD.AssistChip {
                            visible: !!(page.info.hasWorkshop)
                            text: qsTr("Workshop")
                            icon.name: MD.Token.icon.handyman
                        }
                        MD.AssistChip {
                            text: page.info.installKindLabel ?? ""
                            icon.name: MD.Token.icon.install_desktop
                        }
                        MD.AssistChip {
                            visible: page.installSourceCount <= 1
                                     && (page.info.sourceId ?? "") === "steamidra"
                            text: qsTr("Steam CDN · Online Fix")
                            icon.name: MD.Token.icon.check_circle
                            elevated: true
                            mdState.backgroundColor: MD.Token.color.tertiary_container
                            mdState.textColor: MD.Token.color.on_tertiary_container
                            mdState.iconColor: MD.Token.color.on_tertiary_container
                            mdState.outlineColor: MD.Token.color.tertiary_container
                            onClicked: page.openSteamidraTrust()
                        }
                        MD.AssistChip {
                            visible: !!(page.info.hasUpdate)
                            text: qsTr("Update available")
                            icon.name: MD.Token.icon.update
                            elevated: true
                            mdState.backgroundColor: MD.Token.color.tertiary_container
                            mdState.textColor: MD.Token.color.on_tertiary_container
                            mdState.iconColor: MD.Token.color.on_tertiary_container
                            mdState.outlineColor: MD.Token.color.tertiary_container
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small
                        visible: (page.info.sourcePageUrl ?? "").length > 0
                                 || (page.info.sourceWebsiteUrl ?? "").length > 0
                                 || (page.info.steamStoreUrl ?? "").length > 0
                                 || (Core.sources.repositoryUrlFor(page.info.sourceId ?? "") || "").length > 0
                                 || (Core.sources.catalogUrlFor(page.info.sourceId ?? "") || "").length > 0
                                 || (page.gameId || "").length > 0

                        MD.Button {
                            visible: (page.gameId || "").length > 0
                            text: qsTr("Share")
                            icon.name: MD.Token.icon.share
                            mdState.type: MD.Enum.BtText
                            onClicked: shareDialog.open()
                        }

                        MD.Button {
                            visible: (page.info.sourcePageUrl ?? "").length > 0
                                     || (page.info.sourceWebsiteUrl ?? "").length > 0
                            text: (page.info.sourcePageUrl ?? "").length > 0
                                  ? qsTr("Source page")
                                  : qsTr("Source website")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtText
                            onClicked: Core.openExternalUrl(
                                (page.info.sourcePageUrl ?? "").length > 0
                                    ? page.info.sourcePageUrl
                                    : page.info.sourceWebsiteUrl)
                        }

                        MD.Button {
                            visible: (page.info.steamStoreUrl ?? "").length > 0
                            text: qsTr("Steam")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtText
                            onClicked: Core.openExternalUrl(page.info.steamStoreUrl)
                        }

                        MD.Button {
                            visible: (Core.sources.repositoryUrlFor(page.info.sourceId ?? "") || "").length > 0
                            text: qsTr("Plugin source")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtText
                            onClicked: Core.openExternalUrl(
                                Core.sources.repositoryUrlFor(page.info.sourceId ?? ""))
                        }

                        MD.Button {
                            visible: (Core.sources.catalogUrlFor(page.info.sourceId ?? "") || "").length > 0
                            text: qsTr("Catalog URL")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtText
                            onClicked: Core.openExternalUrl(
                                Core.sources.catalogUrlFor(page.info.sourceId ?? ""))
                        }
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

                    ColumnLayout {
                        spacing: MD.Token.spacing.extra_small

                        RowLayout {
                            spacing: MD.Token.spacing.small

                            MD.Button {
                                Layout.alignment: Qt.AlignVCenter
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
                                Layout.alignment: Qt.AlignVCenter
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
                                    if (page.downloadFailed)
                                        Core.retryJob(page.downloadJob.jobId)
                                    else if (page.installFailed || page.readyToInstall)
                                        Core.retryInstall(page.downloadJob.jobId)
                                    else
                                        page.beginInstall()
                                }
                                onPauseToggleRequested: Core.toggleJobPause(page.downloadJob.jobId)
                                onCancelRequested: Core.cancelJob(page.downloadJob.jobId)
                            }

                            MD.IconButton {
                                Layout.alignment: Qt.AlignVCenter
                                readonly property bool favorited: {
                                    const ids = Core.settings.bookmarkedEntryIds
                                    return ids.indexOf(page.gameId) >= 0
                                }
                                checked: favorited
                                mdState.type: MD.Enum.IBtOutlined
                                icon.name: MD.Token.icon.favorite
                                Accessible.name: favorited
                                                 ? qsTr("Remove from favorites")
                                                 : qsTr("Add to favorites")
                                onClicked: Core.toggleBookmark(page.gameId)
                            }

                            MD.IconButton {
                                Layout.alignment: Qt.AlignVCenter
                                visible: hasLaunchLog
                                mdState.type: MD.Enum.IBtOutlined
                                icon.name: MD.Token.icon.receipt_long
                                Accessible.name: qsTr("Launch log")
                                onClicked: openLaunchLog()
                            }

                            MD.Button {
                                Layout.alignment: Qt.AlignVCenter
                                visible: page.playable
                                         || page.downloadComplete
                                         || page.inLibrary
                                text: qsTr("Delete")
                                icon.name: MD.Token.icon.delete
                                mdState.type: MD.Enum.BtOutlined
                                onClicked: removeDialog.open()
                            }

                            MD.IconButton {
                                Layout.alignment: Qt.AlignVCenter
                                visible: page.playable
                                         || page.downloadComplete
                                         || page.inLibrary
                                mdState.type: MD.Enum.IBtOutlined
                                icon.name: MD.Token.icon.settings
                                onClicked: gameSettingsSheet.openForGame(page.gameId)
                            }

                            MD.Button {
                                Layout.alignment: Qt.AlignVCenter
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
                }
            }

            MD.ElevationRectangle {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.preferredHeight: mediaSection.showSection
                                        ? mediaSection.implicitHeight + 2 * MD.Token.spacing.large
                                        : 0
                visible: mediaSection.showSection
                radius: MD.Token.shape.corner.extra_large
                color: MD.Token.color.surface_container
                elevation: MD.Token.elevation.level0

                GameDetailsMediaSection {
                    id: mediaSection
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.large
                    screenshotUrls: page.info.screenshotUrls ?? []
                    trailerUrl: page.info.trailerUrl ?? ""
                    trailerThumbnailUrl: page.info.trailerThumbnailUrl ?? ""
                    loading: page.mediaLoading
                }
            }

            MD.ElevationRectangle {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.preferredHeight: aboutCol.implicitHeight + 2 * MD.Token.spacing.large
                radius: MD.Token.shape.corner.extra_large
                color: MD.Token.color.surface_container
                elevation: MD.Token.elevation.level0

                ColumnLayout {
                    id: aboutCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.large
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        text: qsTr("Description")
                        typescale: MD.Token.typescale.title_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: page.info.description || qsTr("Description is not available yet.")
                        typescale: MD.Token.typescale.body_large
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                    }
                }
            }
        }
    }
    }

    GameSettingsSheet {
        id: gameSettingsSheet
        anchors.fill: parent
    }

    MD.Dialog {
        id: launchLogDialog
        title: qsTr("Launch log")
        modal: true
        width: Math.min(720, page.width > 0 ? page.width - 48 : 720)
        height: Math.min(560, page.height > 0 ? page.height - 48 : 560)

        ColumnLayout {
            width: launchLogDialog.width - launchLogDialog.horizontalPadding * 2
            height: launchLogDialog.height - launchLogDialog.topPadding
                    - launchLogDialog.bottomPadding
                    - (launchLogFooter.implicitHeight > 0 ? launchLogFooter.implicitHeight
                                                         : 0)
                    - MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Why the game may not boot, including the game's own output.")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WordWrap
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    id: launchLogTextArea
                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    persistentSelection: true
                    text: qsTr("No launch has been attempted yet.")
                    padding: 12
                    color: MD.Token.color.on_surface
                    background: Rectangle {
                        color: MD.Token.color.surface_container
                        radius: MD.Token.shape.corner.medium
                    }
                }
            }
        }

        footer: Item {
            id: launchLogFooter
            implicitHeight: launchLogFooterRow.implicitHeight + MD.Token.spacing.medium

            RowLayout {
                id: launchLogFooterRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: MD.Token.spacing.small

                MD.Button {
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Close")
                    onClicked: launchLogDialog.close()
                }

                Item { Layout.fillWidth: true }

                MD.Button {
                    mdState.type: MD.Enum.BtOutlined
                    text: qsTr("Copy")
                    icon.name: MD.Token.icon.content_copy
                    onClicked: Core.copyGameLaunchLog(page.gameId)
                }
                MD.Button {
                    mdState.type: MD.Enum.BtFilled
                    text: qsTr("Save log.txt")
                    icon.name: MD.Token.icon.save
                    onClicked: Core.saveGameLaunchLog(page.gameId)
                }
            }
        }
    }

    MD.Dialog {
        id: shareDialog
        title: qsTr("Share")
        modal: true
        width: Math.min(420, page.width > 0 ? page.width - 48 : 420)

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
                text: qsTr("Suggest to a friend")
                typescale: MD.Token.typescale.title_small
            }

            Repeater {
                model: Core.social.friends

                MD.Button {
                    required property string friendId
                    required property string nickname
                    required property bool online

                    Layout.fillWidth: true
                    text: nickname
                    icon.name: online ? MD.Token.icon.groups : MD.Token.icon.person
                    mdState.type: MD.Enum.BtText
                    onClicked: {
                        Core.suggestGameToFriend(friendId, page.gameId)
                        shareDialog.close()
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
