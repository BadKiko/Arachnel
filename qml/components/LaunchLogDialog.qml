import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.Dialog {
    id: root

    parent: Overlay.overlay
    modal: true
    title: qsTr("Launch log")
    standardButtons: Dialog.NoButton

    width: Math.min(800, parent && parent.width > 0 ? parent.width - 48 * 2 : 800)

    property string gameId: ""
    property string logText: ""

    readonly property string issueLine: {
        const lines = String(root.logText).split(/\r?\n/)
        const keys = ["assertion failed", "fatal error", "crash!!!", "unhandled exception",
                      "access violation"]
        for (let i = 0; i < lines.length; ++i) {
            const line = lines[i].trim()
            if (!line.length)
                continue
            const lower = line.toLowerCase()
            for (let k = 0; k < keys.length; ++k) {
                if (lower.indexOf(keys[k]) >= 0)
                    return line
            }
        }
        return ""
    }

    function openForGame(id) {
        gameId = id || ""
        logText = Core.gameLaunchLog(gameId)
        open()
    }

    onAboutToShow: logText = Core.gameLaunchLog(gameId)

    contentItem: ColumnLayout {
        spacing: MD.Token.spacing.medium
        width: parent ? parent.width : implicitWidth

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Arachnel steps and the game's own output.")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        MD.ElevationRectangle {
            visible: root.issueLine.length > 0
            Layout.fillWidth: true
            implicitHeight: issueRow.implicitHeight + MD.Token.spacing.medium * 2
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.error_container
            elevation: MD.Token.elevation.level0

            RowLayout {
                id: issueRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: MD.Token.spacing.medium
                anchors.rightMargin: MD.Token.spacing.medium
                spacing: MD.Token.spacing.medium

                MD.Icon {
                    Layout.alignment: Qt.AlignTop
                    name: MD.Token.icon.error
                    size: 20
                    color: MD.Token.color.on_error_container
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.extra_small

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("The game reported an error.")
                        color: MD.Token.color.on_error_container
                        typescale: MD.Token.typescale.title_small
                        wrapMode: Text.WordWrap
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.issueLine
                        color: MD.Token.color.on_error_container
                        typescale: MD.Token.typescale.body_small
                        wrapMode: Text.NoWrap
                        elide: Text.ElideMiddle
                    }
                }
            }
        }

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 360
            Layout.minimumHeight: 200
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            elevation: MD.Token.elevation.level0

            ScrollView {
                anchors.fill: parent
                anchors.margins: MD.Token.spacing.small
                clip: true

                TextArea {
                    id: logField
                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    persistentSelection: true
                    text: root.logText
                    color: MD.Token.color.on_surface
                    font.family: "Consolas, Courier New, monospace"
                    font.pixelSize: MD.Token.typescale.body_small.size
                    font.weight: MD.Token.typescale.body_small.weight
                    background: null
                }
            }
        }
    }

    footer: Item {
        implicitHeight: footerBox.implicitHeight + MD.Token.spacing.medium

        MD.DialogButtonBox {
            id: footerBox
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top

            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("Close")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: root.close()
            }

            MD.Button {
                mdState.type: MD.Enum.BtOutlined
                text: qsTr("Copy")
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: Core.copyGameLaunchLog(root.gameId)
            }

            MD.Button {
                mdState.type: MD.Enum.BtFilled
                text: qsTr("Save")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: Core.saveGameLaunchLog(root.gameId)
            }
        }
    }
}
