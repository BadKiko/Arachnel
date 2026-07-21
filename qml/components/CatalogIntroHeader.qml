import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

ColumnLayout {
    spacing: MD.Token.spacing.small

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 4

        MD.Label {
            text: qsTr("Catalog")
            typescale: MD.Token.typescale.headline_medium
        }

        MD.Label {
            Layout.fillWidth: true
            text: Messages.catalogPipelineDesc
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
    }

    CatalogSourceChips {
        Layout.fillWidth: true
    }

    Item {
        Layout.preferredHeight: MD.Token.spacing.extra_small
    }
}
