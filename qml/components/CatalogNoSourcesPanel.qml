import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

Item {
    id: root

    required property var page
    required property int pageMargin

    ColumnLayout {
        anchors.centerIn: parent
        spacing: MD.Token.spacing.medium
        width: Math.min(parent.width - pageMargin * 2, 420)

        SpiderWebMark {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 160
            Layout.preferredHeight: 160
            width: 160
            height: 160
            strokeColor: MD.Token.color.primary
            strokeWidth: 2.5
            opacity: 0.35
        }

        MD.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("No games")
            typescale: MD.Token.typescale.title_large
        }

        MD.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: Messages.catalogConnectHint
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: MD.Token.spacing.small

            MD.Button {
                text: qsTr("Add catalog")
                icon.name: MD.Token.icon.add
                mdState.type: MD.Enum.BtFilled
                onClicked: page.addSourceRequested()
            }

            MD.Button {
                text: qsTr("Settings")
                icon.name: MD.Token.icon.settings
                mdState.type: MD.Enum.BtOutlined
                onClicked: page.openSettings()
            }
        }
    }
}
