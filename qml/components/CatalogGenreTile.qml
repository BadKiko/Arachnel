import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.Card {
    id: root

    property string genreKey: ""
    property bool selected: false
    property string iconName: MD.Token.icon.sports_esports

    implicitWidth: 128
    implicitHeight: 118
    type: root.selected ? MD.Enum.CardFilled : MD.Enum.CardOutlined
    verticalPadding: MD.Token.spacing.small
    horizontalPadding: MD.Token.spacing.small

    contentItem: ColumnLayout {
        spacing: MD.Token.spacing.extra_small

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            radius: MD.Token.shape.corner.medium
            color: root.selected ? MD.Token.color.primary_container : MD.Token.color.surface_container_high

            MD.Icon {
                anchors.centerIn: parent
                name: root.iconName
                size: 32
                color: root.selected ? MD.Token.color.on_primary_container : MD.Token.color.primary
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: root.genreKey.length ? Core.catalogGenreLabel(root.genreKey) : qsTr("Any")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            typescale: MD.Token.typescale.label_medium
            color: MD.Token.color.on_surface
        }
    }
}
