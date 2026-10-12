import QtQuick

import Arachnel.Core 1.0
import Qcm.Material as MD

// Expressive filter trigger: a labelled pill (twin of CatalogSortButton) that turns tonal and
// shows the number of active filters once any are set.
Item {
    id: root

    signal clicked()

    readonly property int activeCount: Core.catalogActiveFilterCount
    readonly property bool active: activeCount > 0

    implicitWidth: pill.implicitWidth
    implicitHeight: 40

    Rectangle {
        id: pill
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: pillRow.implicitWidth + 28
        width: implicitWidth
        height: 40
        radius: tap.pressed ? 12 : height / 2
        color: root.active ? MD.Token.color.secondary_container
             : (hover.hovered ? MD.Token.color.surface_container_highest
                              : MD.Token.color.surface_container_high)

        Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: AppMotion.short } }

        Row {
            id: pillRow
            anchors.centerIn: parent
            spacing: 8

            MD.Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: MD.Token.icon.tune
                size: 20
                color: root.active ? MD.Token.color.on_secondary_container
                                   : MD.Token.color.on_surface_variant
            }
            MD.Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Filters")
                typescale: MD.Token.typescale.label_large
                color: root.active ? MD.Token.color.on_secondary_container
                                   : MD.Token.color.on_surface
            }
            Rectangle {
                visible: root.active
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(20, badge.implicitWidth + 10)
                height: 20
                radius: 10
                color: MD.Token.color.primary

                MD.Label {
                    id: badge
                    anchors.centerIn: parent
                    text: root.activeCount
                    typescale: MD.Token.typescale.label_small
                    color: MD.Token.color.on_primary
                }
            }
        }

        HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
        TapHandler { id: tap; onTapped: root.clicked() }

        Accessible.role: Accessible.Button
        Accessible.name: root.active ? qsTr("Filters, %n active", "", root.activeCount) : qsTr("Filters")
    }
}
