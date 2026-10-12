import QtQuick

import Qcm.Material as MD

// An applied filter shown above the catalog: label plus a close icon. Tapping removes the filter.
Item {
    id: root

    property string text: ""

    signal clicked()

    implicitWidth: row.implicitWidth + 28
    implicitHeight: 32

    Rectangle {
        anchors.fill: parent
        radius: tap.pressed ? 8 : height / 2
        color: hover.hovered ? MD.Token.color.secondary_container
                             : MD.Util.transparent(MD.Token.color.secondary_container, 0.7)
        Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: AppMotion.short } }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6

        MD.Label {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            typescale: MD.Token.typescale.label_large
            color: MD.Token.color.on_secondary_container
        }
        MD.Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: MD.Token.icon.close
            size: 16
            color: MD.Token.color.on_secondary_container
        }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { id: tap; onTapped: root.clicked() }

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Remove filter: %1").arg(root.text)
}
