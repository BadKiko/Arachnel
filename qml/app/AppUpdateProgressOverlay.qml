import QtQuick
import QtQuick.Layouts
import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root
    z: 3000
    visible: Core.appUpdater && Core.appUpdater.downloading

    // Catch clicks so the rest of the UI cannot be used mid-download.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: function(mouse) { mouse.accepted = true }
        onWheel: function(wheel) { wheel.accepted = true }
    }

    Rectangle {
        anchors.fill: parent
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

                MD.LinearIndicator {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 8
                    indeterminate: Core.appUpdater.downloadProgress <= 0
                    running: Core.appUpdater.downloadProgress <= 0
                    from: 0
                    to: 100
                    value: Math.max(0, Core.appUpdater.downloadProgress)
                    strokeWidth: 4
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: (Core.appUpdater.downloadProgress > 0
                           ? (Core.appUpdater.downloadProgress + "%")
                           : qsTr("Starting…"))
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.title_small
                }
            }
        }
    }
}
