import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

ColumnLayout {
    spacing: MD.Token.spacing.small

    MD.Label {
        Layout.fillWidth: true
        text: qsTr("Browse the catalog")
        typescale: MD.Token.typescale.headline_medium
    }

    MD.Label {
        Layout.fillWidth: true
        text: qsTr("Search and filter every game from your enabled sources.")
        wrapMode: Text.WordWrap
        color: MD.Token.color.on_surface_variant
        typescale: MD.Token.typescale.body_medium
    }

    Item {
        Layout.preferredHeight: MD.Token.spacing.extra_small
    }
}
