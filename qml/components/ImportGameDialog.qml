import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

// Add a game that is already on disk (e.g. downloaded elsewhere) to the library.
// The folder is used in place: nothing is copied, moved or deleted.
MD.Dialog {
    id: root

    parent: Overlay.overlay
    title: qsTr("Add existing game")
    modal: true
    width: Math.min(560, parent ? parent.width - 48 : 560)

    signal imported()

    property string folder: ""
    property var info: ({})
    property string selectedId: ""
    property string titleText: ""
    property string errorText: ""

    readonly property bool ready: !!info.ok
    readonly property var candidates: info.candidates || []

    function start() {
        const picked = Core.browseGameFolder()
        if (!picked || !picked.length)
            return
        load(picked)
    }

    function load(path) {
        root.folder = path
        root.errorText = ""
        root.info = Core.inspectGameFolder(path)
        root.titleText = root.info.ok ? root.info.title : ""
        root.selectedId = ""
        // A Steam app id found in the files, or an identical title, is a safe default.
        if (root.info.ok) {
            const wantedId = String(root.info.steamAppId || "")
            const wantedTitle = String(root.info.title || "").toLowerCase()
            for (let i = 0; i < root.candidates.length; ++i) {
                const c = root.candidates[i]
                if (c.inLibrary)
                    continue
                if ((wantedId.length && String(c.steamAppId) === wantedId)
                        || String(c.title).toLowerCase() === wantedTitle) {
                    root.selectedId = c.id
                    root.titleText = c.title
                    break
                }
            }
        }
        root.open()
    }

    function submit() {
        const err = Core.importGameFolder(root.folder, root.selectedId, root.titleText)
        if (err && err.length) {
            root.errorText = err
            return
        }
        root.close()
        root.imported()
    }

    function relativeExe() {
        const exe = String(root.info.executable || "")
        const base = String(root.info.folder || root.folder || "")
        return exe.indexOf(base) === 0 ? exe.substring(base.length).replace(/^[\\/]+/, "") : exe
    }

    contentItem: ColumnLayout {
        spacing: MD.Token.spacing.medium
        width: parent ? parent.width : implicitWidth

        MD.Label {
            Layout.fillWidth: true
            text: root.folder
            elide: Text.ElideMiddle
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
        }

        // Folder can't be used.
        MD.Label {
            Layout.fillWidth: true
            visible: !root.ready
            text: root.info.error || ""
            wrapMode: Text.WordWrap
            color: MD.Token.color.error
            typescale: MD.Token.typescale.body_medium
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.ready
            spacing: MD.Token.spacing.medium

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Arachnel will use this folder as it is. Nothing is copied or deleted.")
                wrapMode: Text.WordWrap
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
            }

            AppTextField {
                Layout.fillWidth: true
                text: root.titleText
                placeholderText: qsTr("Game title")
                onTextEdited: root.titleText = text
            }

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Starts: %1").arg(root.relativeExe())
                elide: Text.ElideMiddle
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_small
            }

            MD.Label {
                Layout.fillWidth: true
                visible: String(root.info.steamAppId || "").length > 0
                text: qsTr("Steam app id found in the files: %1. Online Fix settings are applied when you press Play.")
                          .arg(root.info.steamAppId)
                wrapMode: Text.WordWrap
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_small
            }

            MD.Label {
                Layout.topMargin: MD.Token.spacing.small
                text: root.candidates.length > 0 ? qsTr("Pick the matching game for cover and details")
                                                 : qsTr("No match in the catalog. It will be added as a local game.")
                typescale: MD.Token.typescale.title_small
            }

            Repeater {
                model: root.candidates

                Rectangle {
                    id: row
                    required property var modelData
                    readonly property bool chosen: root.selectedId === modelData.id

                    Layout.fillWidth: true
                    implicitHeight: 56
                    radius: MD.Token.shape.corner.medium
                    color: chosen ? MD.Token.color.secondary_container : MD.Token.color.surface_container
                    border.width: chosen ? 1 : 0
                    border.color: MD.Token.color.primary
                    opacity: modelData.inLibrary ? 0.5 : 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: MD.Token.spacing.small
                        spacing: MD.Token.spacing.medium

                        MD.Icon {
                            name: row.chosen ? MD.Token.icon.check_circle : MD.Token.icon.sports_esports
                            size: 24
                            color: row.chosen ? MD.Token.color.primary : MD.Token.color.on_surface_variant
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            MD.Label {
                                Layout.fillWidth: true
                                text: row.modelData.title
                                elide: Text.ElideRight
                                typescale: MD.Token.typescale.body_large
                            }
                            MD.Label {
                                Layout.fillWidth: true
                                text: row.modelData.inLibrary
                                      ? qsTr("Already in your library")
                                      : (row.modelData.sizeLabel || "")
                                elide: Text.ElideRight
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.label_medium
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: !row.modelData.inLibrary
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (row.chosen) {
                                root.selectedId = ""
                            } else {
                                root.selectedId = row.modelData.id
                                root.titleText = row.modelData.title
                            }
                        }
                    }
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            visible: root.errorText.length > 0
            text: root.errorText
            wrapMode: Text.WordWrap
            color: MD.Token.color.error
            typescale: MD.Token.typescale.body_medium
        }
    }

    footer: Item {
        implicitHeight: importFooter.implicitHeight + MD.Token.spacing.medium

        MD.DialogButtonBox {
            id: importFooter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top

            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: root.close()
            }
            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("Choose another folder")
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: {
                    const picked = Core.browseGameFolder()
                    if (picked && picked.length)
                        root.load(picked)
                }
            }
            MD.Button {
                mdState.type: MD.Enum.BtFilled
                enabled: root.ready && root.titleText.trim().length > 0
                text: qsTr("Add to library")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: root.submit()
            }
        }
    }
}
