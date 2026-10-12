import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// One language of a picker: flag, native name, English name, a check when it is the current one.
Item {
    id: root

    property string code: "en"
    property string name: ""
    property string englishName: ""
    property bool selected: false

    signal clicked()

    implicitHeight: 56
    implicitWidth: 240

    Rectangle {
        anchors.fill: parent
        radius: tap.pressed ? 12 : 28
        color: root.selected ? MD.Token.color.secondary_container
             : (hover.hovered ? MD.Token.color.surface_container_highest : "transparent")
        Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: AppMotion.short } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 16
        spacing: 14

        LanguageFlag {
            code: root.code
            diameter: 30
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            MD.Label {
                Layout.fillWidth: true
                text: root.name
                typescale: MD.Token.typescale.body_large
                color: root.selected ? MD.Token.color.on_secondary_container : MD.Token.color.on_surface
                elide: Text.ElideRight
            }
            MD.Label {
                Layout.fillWidth: true
                visible: text.length > 0 && text !== root.name
                text: root.englishName
                typescale: MD.Token.typescale.body_small
                color: root.selected ? MD.Token.color.on_secondary_container : MD.Token.color.on_surface_variant
                elide: Text.ElideRight
            }
        }

        MD.Icon {
            visible: root.selected
            name: MD.Token.icon.check
            size: 22
            color: MD.Token.color.on_secondary_container
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { id: tap; onTapped: root.clicked() }

    Accessible.role: Accessible.RadioButton
    Accessible.checked: root.selected
    Accessible.name: root.name
}
