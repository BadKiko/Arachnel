import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

// Genre pill: leading icon + label. The selected one fills with primary container and its
// corners round out; pressed squares them off for a springy feel.
Item {
    id: root

    property string genreKey: ""
    property bool selected: false
    property string iconName: MD.Token.icon.sports_esports

    signal clicked()

    implicitWidth: row.implicitWidth + 32
    implicitHeight: 44

    Rectangle {
        anchors.fill: parent
        radius: tap.pressed ? 12 : height / 2
        color: root.selected ? MD.Token.color.primary_container
             : (hover.hovered ? MD.Token.color.surface_container_highest
                              : MD.Token.color.surface_container_high)
        Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: AppMotion.short } }
    }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 8

        MD.Icon {
            name: root.selected ? MD.Token.icon.check : root.iconName
            size: 20
            color: root.selected ? MD.Token.color.on_primary_container : MD.Token.color.on_surface_variant
        }
        MD.Label {
            text: root.genreKey.length ? Core.catalogGenreLabel(root.genreKey) : qsTr("All genres")
            typescale: MD.Token.typescale.label_large
            color: root.selected ? MD.Token.color.on_primary_container : MD.Token.color.on_surface
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { id: tap; onTapped: root.clicked() }

    Accessible.role: Accessible.CheckBox
    Accessible.checked: root.selected
    Accessible.name: row.children[1].text
}
