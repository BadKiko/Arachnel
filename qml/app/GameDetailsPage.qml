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

    readonly property var info: {
        const _rev = root.detailsRevision
        return gameId.length ? Core.entryDetails(gameId) : ({})
    }

    Connections {
        target: Core
        function onEntryMetadataChanged(entryId) {
            if (entryId === root.gameId)
                root.detailsRevision++
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

    Connections {
        target: Core.jobs
        function onJobsChanged() {
            root.refreshDownloadJob()
            root.detailsRevision++
        }
    }

    onGameIdChanged: {
        refreshDownloadJob()
        maybeEnrich()
        if (root.onLinux)
            Core.refreshAvailableProtons()
    }
    onFromCatalogChanged: maybeEnrich()

    function maybeEnrich() {
        if (gameId.length > 0)
            Core.enrichCatalogEntry(gameId)
    }

    Component.onCompleted: {
        refreshDownloadJob()
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

    Flickable {
        id: flick
        anchors.fill: parent
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
                Layout.topMargin: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small

                MD.IconButton {
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.arrow_back
                    onClicked: root.backRequested()
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Game details")
                    typescale: MD.Token.typescale.title_large
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                spacing: MD.Token.spacing.large

                GamePoster {
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: 293
                    Layout.alignment: Qt.AlignTop
                    source: root.info.coverUrl ?? ""
                    fallbackText: (root.info.title ?? "?").charAt(0)
                    cornerRadius: MD.Token.shape.corner.extra_large
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.info.title ?? qsTr("Game not found")
                        typescale: MD.Token.typescale.headline_medium
                        wrapMode: Text.WordWrap
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        visible: !!(root.info.genres)
                        text: root.info.genres ?? ""
                        color: MD.Token.color.primary
                        typescale: MD.Token.typescale.label_large
                        wrapMode: Text.WordWrap
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.AssistChip {
                            text: root.sourceLabel
                            icon.name: MD.Token.icon.storefront
                        }
                        MD.AssistChip {
                            text: "v" + (root.info.version ?? "")
                            icon.name: MD.Token.icon.tag
                        }
                        MD.AssistChip {
                            visible: !!(root.info.sizeLabel)
                            text: root.info.sizeLabel ?? ""
                            icon.name: MD.Token.icon.hard_drive
                        }
                        MD.AssistChip {
                            visible: !!(root.info.hasAddons)
                            text: qsTr("%1 add-ons").arg(root.info.addonCount ?? 0)
                            icon.name: MD.Token.icon.extension
                        }
                        MD.AssistChip {
                            text: root.info.installKindLabel ?? ""
                            icon.name: MD.Token.icon.install_desktop
                        }
                        MD.AssistChip {
                            visible: !!(root.info.hasUpdate)
                            text: qsTr("Update available")
                            icon.name: MD.Token.icon.update
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small
                        visible: (root.info.sourcePageUrl ?? "").length > 0
                                 || (root.info.sourceWebsiteUrl ?? "").length > 0
                                 || (root.info.steamStoreUrl ?? "").length > 0

                        MD.Button {
                            visible: (root.info.sourcePageUrl ?? "").length > 0
                                     || (root.info.sourceWebsiteUrl ?? "").length > 0
                            text: (root.info.sourcePageUrl ?? "").length > 0
                                  ? qsTr("Source page")
                                  : qsTr("Source website")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtText
                            onClicked: Core.openExternalUrl(
                                (root.info.sourcePageUrl ?? "").length > 0
                                    ? root.info.sourcePageUrl
                                    : root.info.sourceWebsiteUrl)
                        }

                        MD.Button {
                            visible: (root.info.steamStoreUrl ?? "").length > 0
                            text: qsTr("Steam")
                            icon.name: MD.Token.icon.open_in_new
                            mdState.type: MD.Enum.BtText
                            onClicked: Core.openExternalUrl(root.info.steamStoreUrl)
                        }
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        visible: root.readyToInstall && !root.installFailed
                        text: Messages.gameInstallTorrentHint
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        visible: root.installFailed
                        text: root.downloadJob.detail || qsTr("Install failed")
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.error
                        typescale: MD.Token.typescale.body_medium
                    }

                    RowLayout {
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            visible: root.playable
                            text: root.isRunning ? qsTr("Stop") : qsTr("Play")
                            icon.name: root.isRunning ? "" : MD.Token.icon.play_arrow
                            mdState.type: MD.Enum.BtFilled
                            mdState.backgroundColor: root.isRunning
                                                 ? MD.Token.color.error
                                                 : MD.Token.color.primary
                            mdState.textColor: root.isRunning
                                               ? MD.Token.color.on_error
                                               : MD.Token.color.on_primary
                            onClicked: root.isRunning
                                         ? Core.stopRunningGame()
                                         : Core.launchGame(root.gameId)
                        }

                        DownloadProgressButton {
                            visible: root.canManageDownload
                            progress: root.downloadJob.progress ?? 0
                            downloading: root.downloadActive
                            paused: root.downloadPaused
                            completed: false
                            readyToInstall: root.readyToInstall
                            installFailed: root.installFailed
                            installing: root.isInstalling
                            onActivated: {
                                if (root.installFailed || root.readyToInstall)
                                    Core.retryInstall(root.downloadJob.jobId)
                                else
                                    root.beginInstall()
                            }
                            onPauseToggleRequested: Core.toggleJobPause(root.downloadJob.jobId)
                        }

                        MD.Button {
                            visible: root.playable
                                     || root.downloadComplete
                                     || root.inLibrary
                            text: qsTr("Delete")
                            icon.name: MD.Token.icon.delete
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: removeDialog.open()
                        }

                        MD.IconButton {
                            visible: root.playable
                                     || root.downloadComplete
                                     || root.inLibrary
                            mdState.type: MD.Enum.IBtOutlined
                            icon.name: MD.Token.icon.settings
                            onClicked: gameSettingsSheet.openForGame(root.gameId)
                        }

                        MD.Button {
                            visible: root.installed && !!(root.info.hasUpdate) && !root.downloadJob.inProgress
                            text: qsTr("Update")
                            icon.name: MD.Token.icon.update
                            mdState.type: MD.Enum.BtFilledTonal
                            onClicked: Core.updateCatalogEntry(root.gameId)
                        }
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        visible: root.showDownloadProgress && !root.downloadCompleted && !!(root.downloadJob.detail)
                        text: root.downloadJob.detail ?? ""
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_medium
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                }
            }

            MD.ElevationRectangle {
                id: mediaCard
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.preferredHeight: mediaLoader.item
                                        ? mediaLoader.item.implicitHeight + 2 * MD.Token.spacing.large
                                        : 0
                visible: mediaLoader.status === Loader.Ready && mediaLoader.item && mediaLoader.item.hasMedia
                radius: MD.Token.shape.corner.extra_large
                color: MD.Token.color.surface_container
                elevation: MD.Token.elevation.level0

                readonly property bool wantsMedia: ((root.info.trailerUrl ?? "").length > 0)
                    || ((root.info.screenshotUrls ?? []).length > 0)

                Loader {
                    id: mediaLoader
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.large
                    active: mediaCard.wantsMedia
                    source: "../components/GameDetailsMediaSection.qml"

                    onLoaded: {
                        if (!item)
                            return
                        item.screenshotUrls = root.info.screenshotUrls ?? []
                        item.trailerUrl = root.info.trailerUrl ?? ""
                    }
                }

                Connections {
                    target: root
                    function onDetailsRevisionChanged() {
                        if (!mediaLoader.item)
                            return
                        mediaLoader.item.screenshotUrls = root.info.screenshotUrls ?? []
                        mediaLoader.item.trailerUrl = root.info.trailerUrl ?? ""
                    }
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
                        text: root.info.description || qsTr("Description is not available yet.")
                        typescale: MD.Token.typescale.body_large
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
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
        id: removeDialog
        title: qsTr("Remove game?")
        modal: true
        width: Math.min(420, root.width > 0 ? root.width - 48 : 420)

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
                        root.confirmRemove()
                    }
                }
            }
        }
    }
}
