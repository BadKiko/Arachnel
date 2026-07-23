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
    contentHeight: body.implicitHeight
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

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: Messages.storageLibrariesDesc
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: driveCol.implicitHeight + MD.Token.spacing.medium * 2

            ColumnLayout {
                id: driveCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small

                Repeater {
                    model: Core.settings.storageLibraries

                    Rectangle {
                        required property string libraryId
                        required property string label
                        required property string path
                        required property bool isDefault

                        Layout.fillWidth: true
                        radius: MD.Token.shape.corner.medium
                        color: root.selectedLibraryId === libraryId
                               ? MD.Token.color.secondary_container
                               : MD.Token.color.surface_container_high
                        border.width: 1
                        border.color: root.selectedLibraryId === libraryId
                                      ? MD.Token.color.primary
                                      : MD.Token.color.outline_variant
                        implicitHeight: driveRow.implicitHeight + MD.Token.spacing.small * 2

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                root.selectedLibraryId = libraryId
                                root.reloadGames()
                            }
                        }

                        RowLayout {
                            id: driveRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: MD.Token.spacing.small
                            spacing: MD.Token.spacing.small

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: MD.Token.spacing.extra_small

                                    MD.Label {
                                        Layout.fillWidth: true
                                        text: label
                                        typescale: MD.Token.typescale.title_small
                                    }

                                    MD.Icon {
                                        visible: isDefault
                                        name: MD.Token.icon.star
                                        size: 18
                                        color: MD.Token.color.primary
                                    }
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: path
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_small
                                    elide: Text.ElideMiddle
                                }
                            }

                            MD.IconButton {
                                visible: !isDefault
                                mdState.type: MD.Enum.IBtStandard
                                icon.name: MD.Token.icon.star
                                onClicked: Core.settings.storageLibraries.setDefaultLibrary(libraryId)
                            }

                            MD.IconButton {
                                visible: Core.settings.storageLibraries.count > 1
                                mdState.type: MD.Enum.IBtStandard
                                icon.name: MD.Token.icon.delete
                                onClicked: {
                                    const id = libraryId
                                    const name = label
                                    const games = Core.gamesOnLibrary(id)
                                    removeDriveDialog.libraryId = id
                                    removeDriveDialog.libraryLabel = name
                                    removeDriveDialog.gameCount = games ? games.length : 0
                                    removeDriveDialog.open()
                                }
                            }
                        }
                    }
                }

                MD.Button {
                    Layout.fillWidth: true
                    mdState.type: MD.Enum.BtOutlined
                    text: qsTr("Add drive…")
                    icon.name: MD.Token.icon.add
                    onClicked: {
                        const folder = Core.browseStorageFolder()
                        if (folder.length)
                            Core.settings.storageLibraries.addLibrary(folder)
                    }
                }

                MD.Button {
                    Layout.fillWidth: true
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Scan for installed games")
                    icon.name: MD.Token.icon.folder_open
                    onClicked: {
                        Core.scanInstalledGames()
                        root.reloadGames()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Games: %1").arg(root.storageGames.length)
                typescale: MD.Token.typescale.title_small
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.small
            visible: root.storageGames.length > 0

            Repeater {
                model: root.storageGames

                Rectangle {
                    required property var modelData

                    Layout.fillWidth: true
                    radius: MD.Token.shape.corner.large
                    color: MD.Token.color.surface_container_low
                    border.width: 1
                    border.color: MD.Token.color.outline_variant
                    implicitHeight: gameRow.implicitHeight + MD.Token.spacing.small * 2

                    RowLayout {
                        id: gameRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: MD.Token.spacing.small
                        spacing: MD.Token.spacing.small

                        GamePoster {
                            Layout.preferredWidth: 56
                            Layout.preferredHeight: 74
                            source: modelData.coverUrl
                            seed: modelData.title
                            fallbackText: modelData.title.charAt(0)
                            cornerRadius: MD.Token.shape.corner.medium
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            MD.Label {
                                Layout.fillWidth: true
                                text: modelData.title
                                typescale: MD.Token.typescale.title_small
                                elide: Text.ElideRight
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: modelData.sizeLabel.length ? modelData.sizeLabel : modelData.installPath
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.body_small
                                elide: Text.ElideMiddle
                            }
                        }

                        MD.CheckBox {
                            checked: root.isGameSelected(modelData.gameId)
                            onToggled: root.toggleGameSelection(modelData.gameId, checked)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.storageGames.length === 0
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            implicitHeight: emptyCol.implicitHeight + MD.Token.spacing.medium * 2

            ColumnLayout {
                id: emptyCol
                anchors.fill: parent
                anchors.margins: MD.Token.spacing.medium

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("No games on this drive yet")
                    typescale: MD.Token.typescale.body_medium
                    color: MD.Token.color.on_surface_variant
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Button {
                Layout.fillWidth: true
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

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtFilledTonal
                enabled: root.selectedGameIds.length > 0
                       && Core.settings.storageLibraries.count > 1
                text: qsTr("Move…")
                icon.name: MD.Token.icon.folder_open
                onClicked: moveSheet.open()
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
                        root.reloadGames()
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
