import QtQuick
import QtQuick.Layouts
import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root
    z: 2900

    readonly property bool busy: (Core.pluginCatalog && Core.pluginCatalog.installing)
                                 || Core.pluginInstallBusy

    visible: busy

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: function(mouse) { mouse.accepted = true }
        onWheel: function(wheel) { wheel.accepted = true }
    }

    Rectangle {
        anchors.fill: parent
        color: MD.Util.transparent(MD.Token.color.scrim, 0.45)

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(400, parent.width - 48)
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container_high
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: col.implicitHeight + MD.Token.spacing.large * 2

            ColumnLayout {
                id: col
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: MD.Token.spacing.large
                spacing: MD.Token.spacing.medium

                MD.Label {
                    Layout.fillWidth: true
                    text: Core.pluginAutoUpdating ? qsTr("Updating plugins…")
                                                  : qsTr("Installing plugins…")
                    typescale: MD.Token.typescale.title_medium
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 8
                    visible: Core.pluginCatalog && Core.pluginCatalog.installing
                             && Core.pluginCatalog.downloadProgress >= 0
                    clip: true

                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: MD.Util.transparent(MD.Token.color.primary, 0.2)
                    }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        width: parent.width * Math.max(0, Math.min(1, Core.pluginCatalog.downloadProgress / 100))
                        radius: 4
                        color: MD.Token.color.primary
                    }
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    visible: Core.pluginCatalog && Core.pluginCatalog.installing
                             && Core.pluginCatalog.downloadProgress > 0
                    text: Core.pluginCatalog.downloadProgress + "%"
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.title_small
                }

                MD.BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.visible && !(Core.pluginCatalog && Core.pluginCatalog.installing
                               && Core.pluginCatalog.downloadProgress > 0)
                    running: visible
                    showDelay: 0
                }
            }
        }
    }
}
