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

        MD.Button {
            mdState.type: MD.Enum.BtFilledTonal
            text: qsTr("Surprise me")
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
            mdState.type: MD.Enum.BtText
            text: qsTr("All games")
            onClicked: root.browseAllRequested()
        }
    }
}
