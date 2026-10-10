import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// A titled card of the game details page. Children go into the content column.
Rectangle {
    id: root

    property string title: ""
    property string iconName: ""
    default property alias content: body.data

    implicitHeight: column.implicitHeight + 2 * MD.Token.spacing.large
    radius: MD.Token.shape.corner.extra_large
    color: MD.Token.color.surface_container

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: MD.Token.spacing.large
        spacing: MD.Token.spacing.medium

        RowLayout {
            Layout.fillWidth: true
            visible: root.title.length > 0
            spacing: MD.Token.spacing.small

            MD.Icon {
                visible: root.iconName.length > 0
                name: root.iconName
                size: 22
                color: MD.Token.color.primary
            }

            MD.Label {
                Layout.fillWidth: true
                text: root.title
                typescale: MD.Token.typescale.title_medium
            }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: MD.Token.spacing.medium
        }
    }
}
