import QtQuick

import Qcm.Material as MD

// Large emphasized page title (v2). A plain Text so it can take any size; the family comes from
// the theme so it matches the rest of the type.
Text {
    id: root

    property int size: 40

    color: MD.Token.color.on_surface
    font.family: fontSource.font.family
    font.pixelSize: size
    font.weight: Font.Bold
    font.letterSpacing: -0.6
    elide: Text.ElideRight
    maximumLineCount: 1

    MD.Label {
        id: fontSource
        visible: false
        typescale: MD.Token.typescale.headline_large
    }
}
