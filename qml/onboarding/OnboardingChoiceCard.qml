import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// A large single-choice card for the wizard: icon, title, one line of explanation. The chosen
// card fills with the secondary container and rounds out; pressing squares it off a little.
Item {
    id: root

    property string iconName: ""
    property string title: ""
    property string description: ""
    property bool selected: false

    signal clicked()

    Layout.fillWidth: true
    implicitHeight: row.implicitHeight + 32

    Rectangle {
        anchors.fill: parent
        radius: tap.pressed ? 14 : 24
        color: root.selected ? MD.Token.color.secondary_container
             : (hover.hovered ? MD.Token.color.surface_container_highest
                              : MD.Token.color.surface_container)
        border.width: root.selected ? 0 : 1
        border.color: MD.Token.color.outline_variant
        Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: AppMotion.short } }
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Rectangle {
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            radius: 22
            color: root.selected ? MD.Token.color.primary : MD.Token.color.surface_container_high

            MD.Icon {
                anchors.centerIn: parent
                name: root.iconName
                size: 24
                color: root.selected ? MD.Token.color.on_primary : MD.Token.color.on_surface_variant
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            MD.Label {
                Layout.fillWidth: true
                text: root.title
                typescale: MD.Token.typescale.title_medium
                color: root.selected ? MD.Token.color.on_secondary_container : MD.Token.color.on_surface
            }
            MD.Label {
                Layout.fillWidth: true
                text: root.description
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WordWrap
                color: root.selected ? MD.Token.color.on_secondary_container
                                     : MD.Token.color.on_surface_variant
            }
        }

        MD.Icon {
            visible: root.selected
            name: MD.Token.icon.check_circle
            fill: true
            size: 24
            color: MD.Token.color.on_secondary_container
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { id: tap; onTapped: root.clicked() }

    Accessible.role: Accessible.RadioButton
    Accessible.checked: root.selected
    Accessible.name: root.title
}
