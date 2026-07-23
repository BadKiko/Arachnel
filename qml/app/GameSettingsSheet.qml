import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal
    dismissOnDragDown: false

    property string gameId: ""
    property int detailsRevision: 0

    readonly property var info: {
        const _rev = root.detailsRevision
        return gameId.length ? Core.entryDetails(gameId) : ({})
    }
    readonly property bool playable: Core.isEntryPlayable(gameId)
    readonly property bool installed: {
        if (root.playable)
            return true
        if (!gameId.length)
            return false
        const lib = Core.library.gameInfo(gameId)
        return ((lib.installPath ?? "")).length > 0
    }
    readonly property bool inLibrary: {
        if (!gameId.length)
            return false
        const lib = Core.library.gameInfo(gameId)
        return (lib.gameId ?? "").length > 0
    }
    readonly property bool onLinux: Qt.platform.os === "linux"
    readonly property bool isInstalling: {
        const job = Core.jobs.jobForEntry(gameId)
        return job.status === "installing"
    }
    readonly property bool readyToInstall: !root.playable
        && (Core.jobs.jobForEntry(gameId).status === "completed")
        && Core.entryDownloadFilesExist(gameId)
    readonly property bool installFailed: ((Core.jobs.jobForEntry(gameId).detail || "")
                                         .indexOf("Install failed") >= 0)
    readonly property string sourceLabel: {
        const sid = info.sourceId ?? ""
        if (!sid.length)
            return ""
        if (info.sourceName && info.sourceName.length && info.sourceName !== sid)
            return info.sourceName
        return Core.sources.nameForId(sid)
    }

    function openForGame(id) {
        gameId = id
        detailsRevision++
        open()
    }

    Connections {
        target: Core.library
        function onLibraryChanged() { root.detailsRevision++ }
    }

    Connections {
        target: Core.jobs
        function onJobsChanged() { root.detailsRevision++ }
    }

    onOpened: {
        if (root.onLinux)
            Core.refreshAvailableProtons()
        launchArgsField.text = root.info.launchArgs ?? ""
        exeField.text = root.info.executableOverride ?? ""
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: MD.Token.spacing.large

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: qsTr("Game settings")
            typescale: MD.Token.typescale.headline_medium
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: !!(root.info.title)
            text: root.info.title ?? ""
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.title_medium
            elide: Text.ElideRight
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: root.installed
            spacing: MD.Token.spacing.medium

            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.medium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Auto-update this game")
                        typescale: MD.Token.typescale.body_large
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("When enabled, updates start automatically after the catalog loads.")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_small
                        wrapMode: Text.WordWrap
                    }
                }

                MD.Switch {
                    checked: root.info.autoUpdate !== false
                    onToggled: Core.setGameAutoUpdate(root.gameId, checked)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.medium
                visible: !!(root.info.onlineFixCanToggle)

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Online Fix for this game")
                        typescale: MD.Token.typescale.body_large
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("When disabled, SteamFix/winmm overlay DLLs are renamed so the game runs without the fix.")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_small
                        wrapMode: Text.WordWrap
                    }
                }

                MD.Switch {
                    checked: !!(root.info.onlineFixEnabled)
                    onToggled: {
                        Core.setGameOnlineFixEnabled(root.gameId, checked)
                        root.detailsRevision++
                    }
                }
            }
        }

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: root.onLinux && root.gameId.length > 0
            Layout.preferredHeight: protonPickCol.implicitHeight + 2 * MD.Token.spacing.medium
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container_low
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: protonPickCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small

                MD.Label {
                    text: qsTr("Proton")
                    typescale: MD.Token.typescale.title_small
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Override Proton for this game. Default uses Settings → Launch.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                    wrapMode: Text.WordWrap
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    MD.FilterChip {
                        text: qsTr("Default")
                        checked: !(root.info.protonId ?? "").length
                        onClicked: Core.setGameProtonId(root.gameId, "")
                    }

                    Repeater {
                        model: Core.availableProtons

                        MD.FilterChip {
                            required property var modelData

                            text: modelData.name
                            checked: (root.info.protonId ?? "") === modelData.id
                            onClicked: Core.setGameProtonId(root.gameId, modelData.id)
                        }
                    }
                }
            }
        }

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: root.playable
            Layout.preferredHeight: launchCol.implicitHeight + 2 * MD.Token.spacing.medium
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container_low
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: launchCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small

                MD.Label {
                    text: qsTr("Launch options")
                    typescale: MD.Token.typescale.title_small
                }

                MD.TextField {
                    id: launchArgsField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Extra launch arguments for this game")
                    onEditingFinished: Core.setGameLaunchArgs(root.gameId, text)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    MD.TextField {
                        id: exeField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Custom executable (optional)")
                        onEditingFinished: Core.setGameExecutableOverride(root.gameId, text)
                    }

                    MD.IconButton {
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.folder_open
                        onClicked: {
                            const path = Core.browseGameExecutable(exeField.text)
                            if (path.length) {
                                exeField.text = path
                                Core.setGameExecutableOverride(root.gameId, path)
                            }
                        }
                    }
                }
            }
        }


        GameSettingsRuntimePanel {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            page: root
        }

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.preferredHeight: metaCol.implicitHeight + 2 * MD.Token.spacing.medium
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container_low
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: metaCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small

                MD.Label {
                    text: qsTr("Information")
                    typescale: MD.Token.typescale.title_small
                }

                Repeater {
                    model: [
                        { label: qsTr("Source"), value: root.sourceLabel },
                        { label: qsTr("Version"), value: root.info.version ?? "" },
                        { label: qsTr("Size"), value: root.info.sizeLabel || "—" },
                        { label: qsTr("Install type"), value: root.info.installKindLabel ?? "" },
                        {
                            label: qsTr("Online Fix"),
                            value: root.info.onlineFixRelevant
                                   ? (root.info.onlineFixLabel || qsTr("Not installed"))
                                   : qsTr("Not needed")
                        },
                        {
                            label: qsTr("Install path"),
                            value: root.playable
                                   ? (root.info.installPath || "—")
                                   : (root.isInstalling
                                          ? qsTr("Installing…")
                                          : root.readyToInstall || root.installFailed
                                          ? qsTr("Waiting to install")
                                          : qsTr("—"))
                        },
                        {
                            label: qsTr("Download"),
                            value: root.info.downloadPath || "—"
                        }
                    ]

                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.Label {
                            Layout.preferredWidth: 120
                            Layout.alignment: Qt.AlignTop
                            text: modelData.label
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: modelData.value
                            typescale: MD.Token.typescale.body_medium
                            wrapMode: Text.WrapAnywhere
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        MD.Button {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            mdState.type: MD.Enum.BtFilled
            text: qsTr("Done")
            onClicked: root.close()
        }
    }
}
