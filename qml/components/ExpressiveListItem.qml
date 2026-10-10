import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// One row of an expressive list group. Rows of a group touch with a 2 px gap; the group's outer
// corners are large and the inner ones small, and a pressed row rounds its inner corners too.
Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconName: ""
    property string shapeKind: "cookie9"
    property color shapeColor: MD.Token.color.secondary_container
    property color shapeIconColor: MD.Token.color.on_secondary_container
    property bool first: false
    property bool last: false
    property bool showChevron: true
    default property alias trailing: trailingSlot.data

    signal clicked()

    implicitHeight: Math.max(72, row.implicitHeight + 2 * MD.Token.spacing.medium)
    implicitWidth: 360

    readonly property real outer: 24
    readonly property real inner: tap.pressed ? 16 : (hover.hovered ? 10 : 4)

    Rectangle {
        anchors.fill: parent
        color: tap.pressed ? MD.Token.color.surface_container_highest
             : hover.hovered ? MD.Token.color.surface_container_high
             : MD.Token.color.surface_container
        topLeftRadius: root.first ? root.outer : root.inner
        topRightRadius: root.first ? root.outer : root.inner
        bottomLeftRadius: root.last ? root.outer : root.inner
        bottomRightRadius: root.last ? root.outer : root.inner

        Behavior on color { ColorAnimation { duration: AppMotion.short } }
        Behavior on topLeftRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on topRightRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on bottomLeftRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on bottomRightRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: MD.Token.spacing.medium
        anchors.rightMargin: MD.Token.spacing.medium
        spacing: MD.Token.spacing.medium

        ShapeIcon {
            visible: root.iconName.length > 0
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            iconName: root.iconName
            kind: root.shapeKind
            color: root.shapeColor
            iconColor: root.shapeIconColor
            interactive: false
            active: hover.hovered
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            MD.Label {
                Layout.fillWidth: true
                text: root.title
                typescale: MD.Token.typescale.title_medium
                elide: Text.ElideRight
            }
            MD.Label {
                Layout.fillWidth: true
                visible: root.subtitle.length > 0
                text: root.subtitle
                typescale: MD.Token.typescale.body_medium
                color: MD.Token.color.on_surface_variant
                wrapMode: Text.WordWrap
            }
        }

        Item {
            id: trailingSlot
            Layout.preferredWidth: childrenRect.width
            Layout.preferredHeight: childrenRect.height
            visible: children.length > 0
        }

        MD.Icon {
            visible: root.showChevron
            name: MD.Token.icon.chevron_right
            size: 24
            color: MD.Token.color.on_surface_variant
            x: 0
            Layout.rightMargin: hover.hovered ? 0 : 4
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        id: tap
        onTapped: root.clicked()
    }
}
