import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// One fact about a game (source, version, size...) as a tonal tile with an icon badge. Lifts a
// little under the pointer and shows `tip` as a tooltip when there is one.
Rectangle {
    id: root

    property string iconName: ""
    property string label: ""
    property string value: ""
    property string tip: ""
    property color accent: MD.Token.color.primary_container
    property color onAccent: MD.Token.color.on_primary_container

    readonly property bool hovered: hover.containsMouse

    implicitHeight: 72
    implicitWidth: 188
    radius: MD.Token.shape.corner.extra_large
    color: hovered ? MD.Token.color.surface_container_highest : MD.Token.color.surface_container_high
    scale: hovered ? 1.025 : 1.0
    transformOrigin: Item.Center

    Behavior on color { ColorAnimation { duration: AppMotion.short } }
    Behavior on scale {
        NumberAnimation {
            duration: AppMotion.short
            easing: AppMotion.emphasizedDecelerate
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: MD.Token.spacing.medium
        anchors.rightMargin: MD.Token.spacing.medium
        spacing: MD.Token.spacing.medium

        Rectangle {
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            radius: MD.Token.shape.corner.large
            color: root.accent

            MD.Icon {
                anchors.centerIn: parent
                name: root.iconName
                size: 24
                color: root.onAccent
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            MD.Label {
                Layout.fillWidth: true
                text: root.label
                typescale: MD.Token.typescale.label_medium
                color: MD.Token.color.on_surface_variant
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            MD.Label {
                Layout.fillWidth: true
                text: root.value
                typescale: MD.Token.typescale.title_medium
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    MD.ToolTip {
        visible: root.hovered && root.tip.length > 0
        text: root.tip
    }
}
