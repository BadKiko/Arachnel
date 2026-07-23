import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    required property var page

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
                    onClicked: page.backRequested()
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
                        text: page.info.title ?? qsTr("Game not found")
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
                            visible: {
                                const g = (page.info.genres ?? "").toString().toLowerCase()
                                return g.split(",").some(t => t.trim() === "drm")
                            }
                            text: qsTr("DRM")
                            icon.name: MD.Token.icon.shield
                            elevated: true
                            mdState.backgroundColor: MD.Token.color.error_container
                            mdState.textColor: MD.Token.color.on_error_container
                            mdState.iconColor: MD.Token.color.on_error_container
                            mdState.outlineColor: MD.Token.color.error_container
                        }
                        MD.AssistChip {
                            visible: !!(page.info.hasAddons)
                            text: qsTr("%1 add-ons").arg(page.info.addonCount ?? 0)
                            icon.name: MD.Token.icon.extension
                        }
                        MD.AssistChip {
                            text: page.info.installKindLabel ?? ""
                            icon.name: MD.Token.icon.install_desktop
                        }
                        MD.AssistChip {
                            visible: (page.info.sourceId ?? "") === "steamidra"
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
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small
                        visible: (page.info.sourcePageUrl ?? "").length > 0
                                 || (page.info.sourceWebsiteUrl ?? "").length > 0
                                 || (page.info.steamStoreUrl ?? "").length > 0

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
                        visible: page.installFailed
                        text: page.downloadJob.detail || qsTr("Install failed")
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.error
                        typescale: MD.Token.typescale.body_medium
                    }

                    RowLayout {
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
                                           : qsTr("PreparingтАж")
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
                            visible: page.canManageDownload
                            progress: page.downloadJob.progress ?? 0
                            bytesDownloaded: Number(page.effectiveDownloaded) || 0
                            totalBytes: Number(page.downloadTotalBytes) || 0
                            detail: page.downloadJob.detail ?? ""
                            downloading: page.downloadActive
                            paused: page.downloadPaused
                            completed: false
                            readyToInstall: page.readyToInstall
                            installFailed: page.installFailed
                            installing: page.isInstalling
                            onActivated: {
                                if (page.installFailed || page.readyToInstall)
                                    Core.retryInstall(page.downloadJob.jobId)
                                else
                                    page.beginInstall()
                            }
                            onPauseToggleRequested: Core.toggleJobPause(page.downloadJob.jobId)
                            onCancelRequested: Core.cancelJob(page.downloadJob.jobId)
                        }

                        MD.Button {
                            visible: page.playable
                                     || page.downloadComplete
                                     || page.inLibrary
                            text: qsTr("Delete")
                            icon.name: MD.Token.icon.delete
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: removeDialog.open()
                        }

                        MD.IconButton {
                            visible: page.playable
                                     || page.downloadComplete
                                     || page.inLibrary
                            mdState.type: MD.Enum.IBtOutlined
                            icon.name: MD.Token.icon.settings
                            onClicked: gameSettingsSheet.openForGame(page.gameId)
                        }

                        MD.Button {
                            visible: page.installed && !!(page.info.hasUpdate) && !page.downloadJob.inProgress
                            text: qsTr("Update")
                            icon.name: MD.Token.icon.update
                            mdState.type: MD.Enum.BtFilledTonal
                            onClicked: Core.updateCatalogEntry(page.gameId)
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

    GameSettingsSheet {
        id: gameSettingsSheet
        anchors.fill: parent
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
}
