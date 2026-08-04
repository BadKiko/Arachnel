import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.Dialog {
    id: root

    parent: Overlay.overlay
    modal: true
    title: qsTr("Update may break DLC")

    property string gameId: ""

    signal updateAccepted(string gameId)

    function openForGame(id) {
        gameId = id || ""
        open()
    }

    contentItem: ColumnLayout {
        spacing: MD.Token.spacing.medium
        width: parent ? parent.width : implicitWidth

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("This update may break the game - DLC for the new build is not on the source yet.")
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
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
                onClicked: root.close()
            }

            MD.Button {
                mdState.type: MD.Enum.BtFilled
                text: qsTr("Update anyway")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    const id = root.gameId
                    root.close()
                    root.updateAccepted(id)
                }
            }
        }
    }
}
