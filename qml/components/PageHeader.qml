import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// Page title block: large headline, optional status line, trailing actions (default slot).
RowLayout {
    id: root

    property string title
    property string subtitle
    property int margin: Appearance.pageMargin
    default property alias trailing: trailingRow.data

    Layout.fillWidth: true
    Layout.leftMargin: root.margin
    Layout.rightMargin: root.margin
    Layout.topMargin: root.margin
    spacing: MD.Token.spacing.medium

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        MD.Label {
            Layout.fillWidth: true
            text: root.title
            typescale: MD.Token.typescale.headline_medium
            elide: Text.ElideRight
        }

        MD.Label {
            Layout.fillWidth: true
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            elide: Text.ElideRight
        }
    }

    RowLayout {
        id: trailingRow
        spacing: MD.Token.spacing.small
    }
}
