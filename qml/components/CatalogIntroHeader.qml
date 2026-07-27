import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

ColumnLayout {
    spacing: MD.Token.spacing.small

    MD.Label {
        Layout.fillWidth: true
        text: qsTr("Discover something to play")
        typescale: MD.Token.typescale.headline_medium
    }

    MD.Label {
        Layout.fillWidth: true
        text: qsTr("Curated shelves from your catalogs — covers and live stats via Steam.")
        wrapMode: Text.WordWrap
        color: MD.Token.color.on_surface_variant
        typescale: MD.Token.typescale.body_medium
    }

    CatalogSourceChips {
        Layout.fillWidth: true
    }

    Item {
        Layout.preferredHeight: MD.Token.spacing.extra_small
    }
}
