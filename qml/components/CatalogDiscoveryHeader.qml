import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var page
    spacing: MD.Token.spacing.small

    signal browseAllRequested()
    signal surpriseRequested()

    RowLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.small

        DisplayTitle {
            text: qsTr("Discover")
        }

        MD.Button {
            Layout.leftMargin: MD.Token.spacing.medium
            mdState.type: MD.Enum.BtFilledTonal
            text: qsTr("Surprise me")
            icon.name: MD.Token.icon.casino
            onClicked: root.surpriseRequested()
        }

        Item { Layout.fillWidth: true }

        MD.IconButton {
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.refresh
            enabled: !Core.catalogDiscovery.loading
            onClicked: Core.catalogDiscovery.refresh()
        }

        MD.Button {
            mdState.type: MD.Enum.BtOutlined
            text: qsTr("All games")
            icon.name: MD.Token.icon.arrow_forward
            onClicked: root.browseAllRequested()
        }
    }
}
