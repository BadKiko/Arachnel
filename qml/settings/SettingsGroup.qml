import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// A titled group of settings rows. The rows touch with 2 px gaps and share one rounded outline:
// big outer corners, small inner ones (SettingsRow works out its own position).
ColumnLayout {
    id: root

    property string title: ""
    property string description: ""
    default property alias rows: rowsCol.data

    Layout.fillWidth: true
    spacing: MD.Token.spacing.small

    MD.Label {
        Layout.fillWidth: true
        Layout.leftMargin: MD.Token.spacing.medium
        visible: root.title.length > 0
        text: root.title
        typescale: MD.Token.typescale.label_large
        color: MD.Token.color.primary
    }

    MD.Label {
        Layout.fillWidth: true
        Layout.leftMargin: MD.Token.spacing.medium
        Layout.rightMargin: MD.Token.spacing.medium
        visible: root.description.length > 0
        text: root.description
        typescale: MD.Token.typescale.body_medium
        color: MD.Token.color.on_surface_variant
        wrapMode: Text.WordWrap
    }

    ColumnLayout {
        id: rowsCol
        Layout.fillWidth: true
        spacing: 2
    }
}
