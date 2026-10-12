import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// One labelled block of the filter sheet: small icon + title row, then the control(s).
ColumnLayout {
    id: root

    property string title: ""
    property string iconName: ""
    property bool modified: false

    default property alias controls: holder.data

    Layout.fillWidth: true
    Layout.leftMargin: MD.Token.spacing.large
    Layout.rightMargin: MD.Token.spacing.large
    spacing: MD.Token.spacing.small

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        MD.Icon {
            visible: root.iconName.length > 0
            name: root.iconName
            size: 18
            color: MD.Token.color.primary
        }
        MD.Label {
            text: root.title
            typescale: MD.Token.typescale.title_small
            color: MD.Token.color.on_surface
        }
        Rectangle {
            visible: root.modified
            Layout.preferredWidth: 6
            Layout.preferredHeight: 6
            radius: 3
            color: MD.Token.color.primary
        }
        Item { Layout.fillWidth: true }
    }

    ColumnLayout {
        id: holder
        Layout.fillWidth: true
        spacing: MD.Token.spacing.small
    }
}
