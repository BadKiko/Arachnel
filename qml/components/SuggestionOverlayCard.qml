import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.ElevationRectangle {
    id: root

    property string friendName: ""
    property string gameId: ""
    property string gameTitle: ""
    property string fallbackCoverUrl: ""

    signal openRequested(string gameId)
    signal dismissed()

    visible: (gameId || "").length > 0
    width: 300
    implicitHeight: contentColumn.implicitHeight + MD.Token.spacing.small * 2

    radius: MD.Token.shape.corner.large
    color: MD.Token.color.surface_container_high
    elevation: MD.Token.elevation.level3
    border.width: 1
    border.color: MD.Token.color.outline_variant
    clip: true

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: MD.Token.spacing.small
        spacing: MD.Token.spacing.small

        GamePoster {
            Layout.fillWidth: true
            Layout.preferredHeight: 112
            source: {
                const fromCatalog = String(Core.suggestionCoverUrl(root.gameId, root.gameTitle) || "")
                if (fromCatalog.length)
                    return fromCatalog
                return String(root.fallbackCoverUrl || "")
            }
            allowRemote: String(source || "").startsWith("http")
            seed: root.gameTitle
            fallbackText: root.gameTitle.charAt(0)
            cornerRadius: MD.Token.shape.corner.large
            decodeWidth: 420
            decodeHeight: 220
            hoverScaleEnabled: false
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            MD.Label {
                Layout.fillWidth: true
                text: root.gameTitle
                typescale: MD.Token.typescale.title_small
                elide: Text.ElideRight
                color: MD.Token.color.on_surface
            }

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Suggested by %1").arg(root.friendName)
                typescale: MD.Token.typescale.body_small
                elide: Text.ElideRight
                color: MD.Token.color.on_surface_variant
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            MD.Button {
                text: qsTr("Open")
                mdState.type: MD.Enum.BtFilledTonal
                onClicked: root.openRequested(root.gameId)
            }

            Item {
                Layout.fillWidth: true
            }

            MD.IconButton {
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.close
                icon.color: MD.Token.color.on_surface_variant
                onClicked: root.dismissed()
            }
        }
    }
}
