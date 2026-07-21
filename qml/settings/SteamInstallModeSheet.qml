import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal

    property string entryId: ""
    property string entryTitle: ""
    property var selectedAddonIds: []
    property string selectedMode: "ddmod"
    property bool rememberChoice: false
    property var continueInstall: function (entryId, title, addonIds, mode) {}

    function openForEntry(id, title, addonIds) {
        entryId = id
        entryTitle = title || ""
        selectedAddonIds = addonIds || []
        const saved = (Core.settings.steamInstallMode || "").toLowerCase()
        if (saved === "ddmod" || saved === "native") {
            selectedMode = saved
            rememberChoice = true
        } else {
            selectedMode = "ddmod"
            rememberChoice = false
        }
        open()
    }

    function pick(mode) {
        selectedMode = mode
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: qsTr("How to install")
            typescale: MD.Token.typescale.headline_medium
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: entryTitle.length
                  ? qsTr("Choose how to get \"%1\".").arg(entryTitle)
                  : qsTr("Choose how to download this game.")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small

            Rectangle {
                Layout.fillWidth: true
                radius: MD.Token.shape.corner.large
                color: root.selectedMode === "ddmod"
                       ? MD.Token.color.secondary_container
                       : MD.Token.color.surface_container
                border.width: 1
                border.color: root.selectedMode === "ddmod"
                              ? MD.Token.color.primary
                              : MD.Token.color.outline_variant
                implicitHeight: arachnelCol.implicitHeight + 2 * MD.Token.spacing.medium

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.pick("ddmod")
                }

                ColumnLayout {
                    id: arachnelCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.medium
                    spacing: 4

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Download in Arachnel")
                        typescale: MD.Token.typescale.title_small
                    }
                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Recommended. Files come from Steam CDN; progress stays in Downloads. Online Fix can be included.")
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_small
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: MD.Token.shape.corner.large
                color: root.selectedMode === "native"
                       ? MD.Token.color.secondary_container
                       : MD.Token.color.surface_container
                border.width: 1
                border.color: root.selectedMode === "native"
                              ? MD.Token.color.primary
                              : MD.Token.color.outline_variant
                implicitHeight: steamCol.implicitHeight + 2 * MD.Token.spacing.medium

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.pick("native")
                }

                ColumnLayout {
                    id: steamCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.medium
                    spacing: 4

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Install via Steam client")
                        typescale: MD.Token.typescale.title_small
                    }
                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("If you prefer Steam’s downloader: Arachnel prepares the game, then Steam fetches the files. Launch from Steam or Arachnel.")
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_small
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Remember choice")
                typescale: MD.Token.typescale.body_large
            }

            MD.Switch {
                checked: root.rememberChoice
                onToggled: root.rememberChoice = checked
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: root.rememberChoice
            text: qsTr("Change later in Settings → Plugins.")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            Item { Layout.fillWidth: true }

            MD.Button {
                text: qsTr("Cancel")
                mdState.type: MD.Enum.BtText
                onClicked: root.close()
            }

            MD.Button {
                text: qsTr("Continue")
                mdState.type: MD.Enum.BtFilled
                onClicked: {
                    if (root.rememberChoice)
                        Core.settings.steamInstallMode = root.selectedMode
                    else
                        Core.settings.steamInstallMode = ""
                    root.close()
                    root.continueInstall(root.entryId, root.entryTitle,
                                         root.selectedAddonIds, root.selectedMode)
                }
            }
        }
    }
}
