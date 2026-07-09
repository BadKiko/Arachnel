import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property string entryId
    required property string title
    required property string coverUrl
    required property string version
    required property string sizeLabel
    required property string installKindLabel

    signal openDetails(string entryId)

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: MD.Token.spacing.small
        spacing: MD.Token.spacing.small

        GamePoster {
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: root.coverUrl
            fallbackText: root.title.charAt(0)
            cornerRadius: MD.Token.shape.corner.extra_large
            onClicked: root.openDetails(root.entryId)
        }

        MD.Label {
            Layout.fillWidth: true
            text: root.title
            typescale: MD.Token.typescale.title_small
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.WordWrap
        }

        MD.Label {
            Layout.fillWidth: true
            text: "v" + root.version + " · " + root.sizeLabel
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_medium
            elide: Text.ElideRight
        }
    }
}
