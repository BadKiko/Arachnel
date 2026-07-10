import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property string gameId
    required property string title
    required property string coverUrl
    required property string sourceName
    required property string version
    required property string installKindLabel
    required property bool hasUpdate

    signal openDetails(string gameId)

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: MD.Token.spacing.small
        spacing: MD.Token.spacing.small

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GamePoster {
                anchors.fill: parent
                source: root.coverUrl
                seed: root.title
                fallbackText: root.title.charAt(0)
                cornerRadius: MD.Token.shape.corner.extra_large
                onClicked: root.openDetails(root.gameId)
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: MD.Token.spacing.small
                visible: root.hasUpdate
                radius: MD.Token.shape.corner.full
                color: MD.Token.color.tertiary_container
                width: updateLabel.implicitWidth + 20
                height: updateLabel.implicitHeight + 12

                MD.Label {
                    id: updateLabel
                    anchors.centerIn: parent
                    text: qsTr("Update")
                    typescale: MD.Token.typescale.label_small
                    color: MD.Token.color.on_tertiary_container
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            text: root.title
            typescale: MD.Token.typescale.title_small
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        MD.Label {
            Layout.fillWidth: true
            text: root.sourceName + " · v" + root.version
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_medium
            elide: Text.ElideRight
        }
    }
}
