import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property string selectedLibraryId: Core.settings.storageLibraries.defaultLibraryId
    property var selectedGameIds: []
    property int libraryRev: 0
    readonly property var storageGames: {
        const _track = libraryRev + Core.library.count
        return Core.gamesOnLibrary(root.selectedLibraryId)
    }

    contentWidth: width
    contentHeight: body.implicitHeight + MD.Token.spacing.extra_large
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    function reloadGames() {
        selectedGameIds = []
        libraryRev++
    }

    function toggleGameSelection(gameId, checked) {
        const ids = selectedGameIds.slice()
        const index = ids.indexOf(gameId)
        if (checked && index < 0)
            ids.push(gameId)
        else if (!checked && index >= 0)
            ids.splice(index, 1)
        selectedGameIds = ids
    }

    function isGameSelected(gameId) {
        return selectedGameIds.indexOf(gameId) >= 0
    }

    Component.onCompleted: reloadGames()

    Connections {
        target: Core.library
        function onCountChanged() { root.reloadGames() }
        function onLibraryChanged() { root.reloadGames() }
    }

    Connections {
        target: Core.settings.storageLibraries
        function onLibrariesChanged() {
            if (Core.settings.storageLibraries.indexOfLibrary(root.selectedLibraryId) < 0)
                root.selectedLibraryId = Core.settings.storageLibraries.defaultLibraryId
            root.reloadGames()
        }
    }

    SettingsColumn {
        id: body
        margin: root.contentMargin

        SettingsGroup {
            title: qsTr("Drives")
            description: Messages.storageLibrariesDesc

            Repeater {
                model: Core.settings.storageLibraries

                delegate: SettingsRow {
                    id: driveRow
                    required property string libraryId
                    required property string label
                    required property string path
                    required property bool isDefault
                    readonly property bool picked: root.selectedLibraryId === libraryId

                    title: label + (isDefault ? "  ★" : "")
                    subtitle: path
                    iconName: MD.Token.icon.hard_drive
                    clickable: true
                    titleColor: picked ? MD.Token.color.primary : MD.Token.color.on_surface
                    onClicked: {
                        root.selectedLibraryId = libraryId
                        root.reloadGames()
                    }

                    MD.Icon {
                        visible: driveRow.picked
                        name: MD.Token.icon.check_circle
                        size: 22
                        color: MD.Token.color.primary
                    }

                    MD.IconButton {
                        visible: !driveRow.isDefault
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.star
                        Accessible.name: qsTr("Make default")
                        onClicked: Core.settings.storageLibraries.setDefaultLibrary(driveRow.libraryId)
                    }

                    MD.IconButton {
                        visible: Core.settings.storageLibraries.count > 1
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.delete
                        onClicked: {
                            const games = Core.gamesOnLibrary(driveRow.libraryId)
                            removeDriveDialog.libraryId = driveRow.libraryId
                            removeDriveDialog.libraryLabel = driveRow.label
                            removeDriveDialog.gameCount = games ? games.length : 0
                            removeDriveDialog.open()
                        }
                    }
                }
            }

            SettingsRow {
                title: qsTr("Add drive…")
                iconName: MD.Token.icon.add
                clickable: true
                showChevron: true
                onClicked: {
                    const folder = Core.browseStorageFolder()
                    if (folder.length)
                        Core.settings.storageLibraries.addLibrary(folder)
                }
            }

            SettingsRow {
                title: qsTr("Scan for installed games")
                iconName: MD.Token.icon.folder_open
                clickable: true
                showChevron: true
                onClicked: {
                    Core.scanInstalledGames()
                    root.reloadGames()
                }
            }
        }

        SettingsGroup {
            title: qsTr("Games: %1").arg(root.storageGames.length)

            SettingsRow {
                visible: root.storageGames.length === 0
                title: qsTr("No games on this drive yet")
                iconName: MD.Token.icon.sports_esports
            }

            Repeater {
                model: root.storageGames

                delegate: SettingsRow {
                    id: gameRowItem
                    required property var modelData
                    title: modelData.title
                    subtitle: modelData.sizeLabel.length ? modelData.sizeLabel : modelData.installPath
                    iconName: MD.Token.icon.sports_esports
                    clickable: true
                    onClicked: root.toggleGameSelection(modelData.gameId, !root.isGameSelected(modelData.gameId))

                    MD.CheckBox {
                        checked: root.isGameSelected(gameRowItem.modelData.gameId)
                        onToggled: root.toggleGameSelection(gameRowItem.modelData.gameId, checked)
                    }
                }
            }

            // Actions for the ticked games.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: MD.Token.spacing.small
                visible: root.storageGames.length > 0
                spacing: MD.Token.spacing.small

                MD.Button {
                    mdState.type: MD.Enum.BtFilledTonal
                    enabled: root.selectedGameIds.length > 0
                             && Core.settings.storageLibraries.count > 1
                    text: qsTr("Move…")
                    icon.name: MD.Token.icon.drive_file_move
                    onClicked: moveSheet.open()
                }

                MD.Button {
                    mdState.type: MD.Enum.BtOutlined
                    enabled: root.selectedGameIds.length > 0
                    text: qsTr("Delete")
                    icon.name: MD.Token.icon.delete
                    onClicked: {
                        for (let i = 0; i < root.selectedGameIds.length; ++i)
                            Core.removeGame(root.selectedGameIds[i], true)
                        root.reloadGames()
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }
    }

    MD.BottomSheet {
        id: moveSheet
        sheetType: MD.Enum.BottomSheetModal
        property string targetLibraryId: ""

        ColumnLayout {
            width: moveSheet.sheetWidth
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                Layout.margins: MD.Token.spacing.large
                text: qsTr("Move to drive")
                typescale: MD.Token.typescale.title_large
            }

            Repeater {
                model: Core.settings.storageLibraries

                MD.Button {
                    required property string libraryId
                    required property string label

                    Layout.fillWidth: true
                    Layout.leftMargin: MD.Token.spacing.large
                    Layout.rightMargin: MD.Token.spacing.large
                    visible: libraryId !== root.selectedLibraryId
                    text: label
                    mdState.type: MD.Enum.BtFilledTonal
                    onClicked: {
                        for (let i = 0; i < root.selectedGameIds.length; ++i)
                            Core.moveGame(root.selectedGameIds[i], libraryId)
                        moveSheet.close()
                        root.selectedGameIds = []
                    }
                }
            }
        }
    }

    MD.Dialog {
        id: removeDriveDialog
        parent: Overlay.overlay
        modal: true
        width: Math.min(440, Overlay.overlay ? Overlay.overlay.width - 48 : 440)
        property string libraryId: ""
        property string libraryLabel: ""
        property int gameCount: 0
        title: qsTr("Remove drive?")

        contentItem: ColumnLayout {
            spacing: MD.Token.spacing.medium
            width: removeDriveDialog.width - removeDriveDialog.horizontalPadding * 2

            MD.Label {
                Layout.fillWidth: true
                text: removeDriveDialog.gameCount > 0
                      ? qsTr("“%1” still has games (%2). Remove the drive from Arachnel anyway? Files stay on disk; games stay in the library under another drive.")
                            .arg(removeDriveDialog.libraryLabel)
                            .arg(removeDriveDialog.gameCount)
                      : qsTr("Remove “%1” from Arachnel? Files on disk are not deleted.")
                            .arg(removeDriveDialog.libraryLabel)
                wrapMode: Text.WordWrap
                typescale: MD.Token.typescale.body_medium
                color: MD.Token.color.on_surface_variant
            }
        }

        footer: Item {
            implicitHeight: footerRow.implicitHeight + MD.Token.spacing.medium

            MD.DialogButtonBox {
                id: footerRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top

                MD.Button {
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Cancel")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                    onClicked: removeDriveDialog.close()
                }

                MD.Button {
                    mdState.type: MD.Enum.BtFilled
                    text: removeDriveDialog.gameCount > 0 ? qsTr("Remove anyway") : qsTr("Remove")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    onClicked: {
                        const id = removeDriveDialog.libraryId
                        const force = removeDriveDialog.gameCount > 0
                        removeDriveDialog.close()
                        if (!Core.removeStorageLibrary(id, force))
                            return
                        if (root.selectedLibraryId === id)
                            root.selectedLibraryId = Core.settings.storageLibraries.defaultLibraryId
                        root.reloadGames()
                    }
                }
            }
        }
    }
}
