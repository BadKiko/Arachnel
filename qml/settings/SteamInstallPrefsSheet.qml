import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal

    function openPrefs() {
        open()
    }

    readonly property string currentMode: (Core.settings.steamInstallMode || "").toLowerCase()

    function pick(mode) {
        Core.settings.steamInstallMode = mode
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: qsTr("Steam install method")
            typescale: MD.Token.typescale.headline_small
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: qsTr("Ask each time, or always use Arachnel CDN / Steam client.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small

            Repeater {
                model: [
                    {
                        mode: "",
                        title: qsTr("Ask each time"),
                        body: qsTr("Show the install choice before every Steam game.")
                    },
                    {
                        mode: "ddmod",
                        title: qsTr("Arachnel (CDN)"),
                        body: qsTr("Always download via Arachnel from Steam CDN. Works without a Steam Store license.")
                    },
                    {
                        mode: "native",
                        title: qsTr("Steam client"),
                        body: qsTr("Always hand off to Steam. LumaCore unlocks download even if the game is not on your account.")
                    }
                ]

                Rectangle {
                    required property var modelData

                    Layout.fillWidth: true
                    radius: MD.Token.shape.corner.large
                    color: root.currentMode === modelData.mode
                           ? MD.Token.color.secondary_container
                           : MD.Token.color.surface_container
                    border.width: 1
                    border.color: root.currentMode === modelData.mode
                                  ? MD.Token.color.primary
                                  : MD.Token.color.outline_variant
                    implicitHeight: optCol.implicitHeight + 2 * MD.Token.spacing.medium

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.pick(modelData.mode)
                    }

                    ColumnLayout {
                        id: optCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: MD.Token.spacing.medium
                        spacing: 4

                        MD.Label {
                            Layout.fillWidth: true
                            text: modelData.title
                            typescale: MD.Token.typescale.title_small
                        }
                        MD.Label {
                            Layout.fillWidth: true
                            text: modelData.body
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_small
                        }
                    }
                }
            }
        }

        MD.Button {
            Layout.alignment: Qt.AlignRight
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            text: qsTr("Done")
            mdState.type: MD.Enum.BtFilled
            onClicked: root.close()
        }
    }
}
