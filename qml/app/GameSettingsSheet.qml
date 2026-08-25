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
    readonly property var availableLaunchOptions: root.gameId.length ? Core.gameLaunchOptions(root.gameId) : []
    readonly property string currentSelectedLaunchOption: root.info.selectedLaunchOptionId ?? ""
    readonly property var installedComponents: {
        const _rev = root.detailsRevision
        const raw = root.info.components
        if (raw === undefined || raw === null)
            return []
        const len = raw.length !== undefined ? raw.length : 0
        const out = []
        for (let i = 0; i < len; ++i) {
            const c = raw[i]
            if (c && c.installed)
                out.push(c)
        }
        return out
    }

    /** Installed Steam/plugin DLC components for this game. */
    readonly property int installedDlcCount: {
        const _rev = root.detailsRevision
        const raw = root.info.components
        const len = raw && raw.length !== undefined ? raw.length : 0
        let n = 0
        for (let i = 0; i < len; ++i) {
            const c = raw[i]
            if (c && c.installed)
                ++n
        }
        return n
    }
    readonly property bool isInstalling: {
        const job = Core.jobs.jobForEntry(gameId)
        return job.status === "installing"
    }
    readonly property bool readyToInstall: !root.playable
        && (Core.jobs.jobForEntry(gameId).status === "completed")
        && Core.entryDownloadFilesExist(gameId)
    readonly property bool downloadFailed: {
        const job = Core.jobs.jobForEntry(gameId)
        return job.status === "failed" || job.status === "cancelled"
    }
    readonly property bool installFailed: {
        const job = Core.jobs.jobForEntry(gameId)
        return !root.downloadFailed && !!(job.installFailed)
    }
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
        if (Core.healInstalledAddons)
            Core.healInstalledAddons(id)
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

        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: root.installed
            spacing: MD.Token.spacing.small

            MD.Button {
                mdState.type: MD.Enum.BtFilledTonal
                text: qsTr("Desktop shortcut")
                icon.name: MD.Token.icon.desktop_windows
                onClicked: Core.createGameDesktopShortcut(root.gameId)
            }

            MD.Button {
                mdState.type: MD.Enum.BtFilledTonal
                text: qsTr("Start menu shortcut")
                icon.name: MD.Token.icon.apps
                onClicked: Core.createGameStartMenuShortcut(root.gameId)
            }

            MD.Button {
                mdState.type: MD.Enum.BtFilledTonal
                text: qsTr("Add to Steam")
                icon.name: MD.Token.icon.sports_esports
                onClicked: Core.addGameToSteam(root.gameId)
            }
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
                        text: qsTr("Runs without a Steam license. Steam may not show you as in-game.")
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
            implicitHeight: protonPickCol.implicitHeight + 2 * MD.Token.spacing.medium
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
            implicitHeight: launchCol.implicitHeight + 2 * MD.Token.spacing.medium
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container_low
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: launchCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.medium

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Launch options")
                    typescale: MD.Token.typescale.title_small
                }

                AppTextField {
                    id: launchArgsField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Extra launch arguments for this game")
                    onEditingFinished: Core.setGameLaunchArgs(root.gameId, text)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    AppTextField {
                        id: exeField
                        Layout.fillWidth: true
                        placeholderText: (root.info.defaultExecutableName ?? "").length > 0
                                         ? qsTr("Default: %1").arg(root.info.defaultExecutableName)
                                         : qsTr("Custom executable (optional)")
                        onEditingFinished: Core.setGameExecutableOverride(root.gameId, text)
                    }

                    MD.IconButton {
                        Layout.alignment: Qt.AlignVCenter
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.folder_open
                        onClicked: {
                            const path = Core.browseGameExecutable(
                                exeField.text, root.info.installPath || "")
                            if (path.length) {
                                exeField.text = path
                                Core.setGameExecutableOverride(root.gameId, path)
                            }
                        }
                    }
                }

                MD.Label {
                    Layout.fillWidth: true
                    visible: (root.info.defaultExecutable ?? "").length > 0 && !exeField.text.length
                    text: qsTr("Default executable: %1").arg(root.info.defaultExecutable)
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                    elide: Text.ElideMiddle
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.extra_small
                    visible: root.availableLaunchOptions.length === 1

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Launch mode")
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.on_surface_variant
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: singleOptRow.implicitHeight + MD.Token.spacing.small * 2
                        radius: MD.Token.shape.corner.small
                        color: MD.Token.color.surface_container
                        border.width: 1
                        border.color: MD.Token.color.outline_variant

                        RowLayout {
                            id: singleOptRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: MD.Token.spacing.small
                            spacing: MD.Token.spacing.small

                            MD.Icon {
                                name: MD.Token.icon.play_circle
                                size: 20
                                color: MD.Token.color.primary
                                Layout.alignment: Qt.AlignVCenter
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: root.availableLaunchOptions[0].title || qsTr("Default")
                                    typescale: MD.Token.typescale.body_medium
                                    color: MD.Token.color.on_surface
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: {
                                        const opt = root.availableLaunchOptions[0]
                                        let desc = opt.executable || ""
                                        if (opt.arguments && opt.arguments.length > 0)
                                            desc += " " + (Array.isArray(opt.arguments) ? opt.arguments.join(" ") : opt.arguments)
                                        return desc
                                    }
                                    typescale: MD.Token.typescale.body_small
                                    color: MD.Token.color.on_surface_variant
                                    elide: Text.ElideMiddle
                                    visible: text.length > 0
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small
                    visible: root.availableLaunchOptions.length > 1

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Default launch mode")
                        typescale: MD.Token.typescale.label_large
                        color: MD.Token.color.on_surface_variant
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.extra_small

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: askRow.implicitHeight + MD.Token.spacing.small * 2
                            radius: MD.Token.shape.corner.small
                            readonly property bool isSelected: root.currentSelectedLaunchOption === ""
                            color: isSelected ? MD.Token.color.secondary_container : (askMouse.containsMouse ? MD.Token.color.surface_container_high : MD.Token.color.surface_container)
                            border.width: isSelected ? 2 : 1
                            border.color: isSelected ? MD.Token.color.primary : MD.Token.color.outline_variant

                            MouseArea {
                                id: askMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: Core.setGameSelectedLaunchOption(root.gameId, "")
                            }

                            RowLayout {
                                id: askRow
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: MD.Token.spacing.small
                                spacing: MD.Token.spacing.small

                                Rectangle {
                                    width: 16
                                    height: 16
                                    radius: 8
                                    border.width: 2
                                    border.color: parent.parent.isSelected ? MD.Token.color.primary : MD.Token.color.outline
                                    color: "transparent"
                                    Layout.alignment: Qt.AlignVCenter
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: MD.Token.color.primary
                                        visible: parent.parent.parent.isSelected
                                    }
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Always ask before launch")
                                    typescale: MD.Token.typescale.body_medium
                                    color: parent.parent.isSelected ? MD.Token.color.on_secondary_container : MD.Token.color.on_surface
                                }
                            }
                        }

                        Repeater {
                            model: root.availableLaunchOptions

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: optSetRow.implicitHeight + MD.Token.spacing.small * 2
                                radius: MD.Token.shape.corner.small
                                readonly property bool isSelected: root.currentSelectedLaunchOption === modelData.id
                                color: isSelected ? MD.Token.color.secondary_container : (optSetMouse.containsMouse ? MD.Token.color.surface_container_high : MD.Token.color.surface_container)
                                border.width: isSelected ? 2 : 1
                                border.color: isSelected ? MD.Token.color.primary : MD.Token.color.outline_variant

                                MouseArea {
                                    id: optSetMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Core.setGameSelectedLaunchOption(root.gameId, modelData.id)
                                }

                                RowLayout {
                                    id: optSetRow
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: MD.Token.spacing.small
                                    spacing: MD.Token.spacing.small

                                    Rectangle {
                                        width: 16
                                        height: 16
                                        radius: 8
                                        border.width: 2
                                        border.color: parent.parent.isSelected ? MD.Token.color.primary : MD.Token.color.outline
                                        color: "transparent"
                                        Layout.alignment: Qt.AlignVCenter
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 8
                                            height: 8
                                            radius: 4
                                            color: MD.Token.color.primary
                                            visible: parent.parent.parent.isSelected
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        MD.Label {
                                            Layout.fillWidth: true
                                            text: modelData.title || qsTr("Option %1").arg(modelData.id)
                                            typescale: MD.Token.typescale.body_medium
                                            color: parent.parent.parent.isSelected ? MD.Token.color.on_secondary_container : MD.Token.color.on_surface
                                            font.bold: parent.parent.parent.isSelected
                                        }

                                        MD.Label {
                                            Layout.fillWidth: true
                                            text: {
                                                const args = (modelData.arguments && modelData.arguments.length)
                                                    ? " " + modelData.arguments.join(" ") : ""
                                                const exeName = (modelData.executable || "").split("/").pop().split("\\").pop()
                                                return exeName + args
                                            }
                                            typescale: MD.Token.typescale.body_small
                                            color: MD.Token.color.on_surface_variant
                                            visible: text.length > 0
                                        }
                                    }
                                }
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
            implicitHeight: metaCol.implicitHeight + 2 * MD.Token.spacing.medium
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
                        { label: qsTr("Size"), value: root.info.sizeLabel || "-" },
                        {
                            label: qsTr("DLC"),
                            value: root.installedDlcCount > 0
                                   ? String(root.installedDlcCount)
                                   : qsTr("None")
                        },
                        { label: qsTr("Install type"), value: root.info.installKindLabel ?? "" },
                        {
                            label: qsTr("Online Fix"),
                            value: root.info.onlineFixRelevant
                                   ? (root.info.onlineFixLabel || qsTr("Not installed"))
                                   : qsTr("Not needed")
                        },
                        {
                            label: qsTr("Steamless"),
                            value: root.info.steamlessRelevant
                                   ? (root.info.steamlessLabel || qsTr("Not needed"))
                                   : qsTr("Not needed")
                        },
                        {
                            label: qsTr("Install path"),
                            value: root.playable
                                   ? (root.info.installPath || "-")
                                   : (root.isInstalling
                                          ? qsTr("Installing…")
                                          : root.readyToInstall || root.installFailed
                                            || root.downloadFailed
                                          ? qsTr("Waiting to install")
                                          : qsTr("-"))
                        },
                        {
                            label: qsTr("Download"),
                            value: root.info.downloadPath || "-"
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
