import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// One row of a SettingsGroup: optional icon, title and supporting text, a trailing control, and
// optional content under the text (chips, colour swatches...). Corners follow the row's place in
// its group; a clickable row lights up and rounds a little under the pointer.
Item {
    id: root

    property string title: ""
    property string subtitle: ""
    property string iconName: ""
    property bool clickable: false
    property bool showChevron: false
    property color titleColor: MD.Token.color.on_surface
    default property alias trailing: trailingRow.data
    property alias below: belowCol.data

    signal clicked()

    Layout.fillWidth: true
    implicitHeight: Math.max(64, body.implicitHeight + 2 * 14)

    readonly property var siblings: parent ? Array.from(parent.visibleChildren).filter(c => c.height > 0) : []
    readonly property bool isFirst: siblings.length > 0 && siblings[0] === root
    readonly property bool isLast: siblings.length > 0 && siblings[siblings.length - 1] === root
    readonly property real outer: 24
    readonly property real inner: root.clickable && tap.pressed ? 16 : (root.clickable && hover.hovered ? 10 : 4)

    Rectangle {
        anchors.fill: parent
        color: root.clickable && hover.hovered ? MD.Token.color.surface_container_high
                                               : MD.Token.color.surface_container
        topLeftRadius: root.isFirst ? root.outer : root.inner
        topRightRadius: root.isFirst ? root.outer : root.inner
        bottomLeftRadius: root.isLast ? root.outer : root.inner
        bottomRightRadius: root.isLast ? root.outer : root.inner

        Behavior on color { ColorAnimation { duration: AppMotion.short } }
        Behavior on topLeftRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on topRightRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on bottomLeftRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on bottomRightRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
    }

    RowLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: MD.Token.spacing.large
        anchors.rightMargin: MD.Token.spacing.medium
        spacing: MD.Token.spacing.medium

        Rectangle {
            visible: root.iconName.length > 0
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignTop
            radius: 20
            color: MD.Token.color.secondary_container

            MD.Icon {
                anchors.centerIn: parent
                name: root.iconName
                size: 22
                color: MD.Token.color.on_secondary_container
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            MD.Label {
                Layout.fillWidth: true
                visible: root.title.length > 0
                text: root.title
                typescale: MD.Token.typescale.title_medium
                color: root.titleColor
                wrapMode: Text.WordWrap
            }
            MD.Label {
                Layout.fillWidth: true
                visible: root.subtitle.length > 0
                text: root.subtitle
                typescale: MD.Token.typescale.body_medium
                color: MD.Token.color.on_surface_variant
                wrapMode: Text.WordWrap
            }
            ColumnLayout {
                id: belowCol
                Layout.fillWidth: true
                Layout.topMargin: children.length > 0 ? MD.Token.spacing.small : 0
                spacing: MD.Token.spacing.small
                visible: children.length > 0
            }
        }

        RowLayout {
            id: trailingRow
            Layout.alignment: Qt.AlignVCenter
            spacing: MD.Token.spacing.small
        }

        MD.Icon {
            visible: root.showChevron
            name: MD.Token.icon.chevron_right
            size: 24
            color: MD.Token.color.on_surface_variant
        }
    }

    HoverHandler {
        id: hover
        enabled: root.clickable
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        id: tap
        enabled: root.clickable
        onTapped: root.clicked()
    }
}
