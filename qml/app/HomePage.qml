import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

Item {
    id: root

    ColumnLayout {
        anchors.centerIn: parent
        spacing: MD.Token.spacing.large

        MD.Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Arachnel")
            typescale: MD.Token.typescale.headline_medium
            color: MD.Token.color.primary
        }

        MD.Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("QML Material 3")
            typescale: MD.Token.typescale.body_large
            color: MD.Token.color.on_surface_variant
        }

        MD.Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Нажми меня")
            mdState.type: MD.Enum.BtFilled
            mdState.backgroundColor: MD.Token.color.primary
            mdState.textColor: MD.Token.color.on_primary
            onClicked: console.log("Arachnel: button clicked")
        }
    }
}
