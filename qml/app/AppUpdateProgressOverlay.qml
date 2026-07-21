import QtQuick
import QtQuick.Layouts
import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    Rectangle {
        anchors.fill: parent
        visible: Core.appUpdater && Core.appUpdater.downloading
        z: 2000
        color: MD.Util.transparent(MD.Token.color.scrim, 0.55)

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(420, parent.width - 48)
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container_high
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: updateColumn.implicitHeight + MD.Token.spacing.large * 2

            ColumnLayout {
                id: updateColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: MD.Token.spacing.large
                spacing: MD.Token.spacing.medium

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Downloading Arachnel update…")
                    typescale: MD.Token.typescale.title_medium
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Please wait. The installer will open automatically.")
                    wrapMode: Text.WordWrap
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    clip: true

                    Rectangle {
                        anchors.fill: parent
                        radius: 3
                        color: MD.Util.transparent(MD.Token.color.primary, 0.2)
                    }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        width: parent.width * (Core.appUpdater.downloadProgress / 100)
                        radius: 3
                        color: MD.Token.color.primary
                    }
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: Core.appUpdater.downloadProgress + "%"
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_small
                }
            }
        }
    }
}
