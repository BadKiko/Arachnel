import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

ColumnLayout {
    id: root

    // 0 = expanded title, 1 = scrolled away / handing off to compact bar.
    property real collapseProgress: 0

    spacing: MD.Token.spacing.small
    opacity: 1 - root.collapseProgress * 0.95
    scale: 1 - root.collapseProgress * 0.06
    transformOrigin: Item.TopLeft

    transform: Translate {
        y: -14 * root.collapseProgress
    }

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
        opacity: 1 - root.collapseProgress
    }

    Item {
        Layout.preferredHeight: MD.Token.spacing.extra_small
    }
}
