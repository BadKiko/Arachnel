import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

Item {
    id: root

    property string title: ""
    property string value: ""
    property string iconName: ""
    property string shapeKind: "cookie9"

    implicitHeight: 88
    Layout.fillWidth: true
    Layout.minimumHeight: 88

    HoverHandler { id: cardHover }

    MD.ElevationRectangle {
        anchors.fill: parent
        radius: cardHover.hovered ? 32 : 24
        color: cardHover.hovered ? MD.Token.color.surface_container_high : MD.Token.color.surface_container
        elevation: MD.Token.elevation.level0
        Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: AppMotion.short } }

        RowLayout {
            anchors.fill: parent
            anchors.margins: MD.Token.spacing.medium
            spacing: MD.Token.spacing.medium

            ShapeIcon {
                Layout.preferredWidth: 52
                Layout.preferredHeight: 52
                Layout.alignment: Qt.AlignVCenter
                iconName: root.iconName
                kind: root.shapeKind
                iconSize: 24
                active: cardHover.hovered
                interactive: false
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                MD.Label {
                    Layout.fillWidth: true
                    text: root.value
                    typescale: MD.Token.typescale.headline_medium
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: root.title
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_large
                    elide: Text.ElideRight
                }
            }
        }
    }
}
