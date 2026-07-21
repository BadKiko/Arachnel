import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property var page
    required property int pageMargin

    Item {
        id: emptyResultsCard
        anchors.centerIn: parent
        width: Math.min(parent.width - pageMargin * 2, 720)
        height: 220
        clip: true

        layer.enabled: true
        layer.effect: MD.RoundClip {
            corners: MD.Util.corners(page.cardRadius)
            size: Qt.vector2d(emptyResultsCard.width, emptyResultsCard.height)
        }

        Rectangle {
            anchors.fill: parent
            color: MD.Token.color.surface_container_high
        }

        SpiderWebMark {
            width: 260
            height: 260
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: -130
            strokeColor: MD.Token.color.primary
            strokeWidth: 2.5
            opacity: 0.18
        }

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: MD.Token.spacing.extra_large
            anchors.rightMargin: MD.Token.spacing.extra_large + 80
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                text: page.noSourceSelected
                      ? qsTr("Select sources")
                      : qsTr("Nothing found")
                typescale: MD.Token.typescale.title_large
            }

            MD.Label {
                Layout.fillWidth: true
                text: page.noSourceSelected
                      ? Messages.catalogEnableChipsHint
                      : qsTr("Try another search or refresh the catalog.")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WordWrap
            }

            MD.Button {
                visible: !page.noSourceSelected
                text: qsTr("Refresh")
                icon.name: MD.Token.icon.refresh
                mdState.type: MD.Enum.BtFilledTonal
                onClicked: Core.refreshCatalog(Core.activeCatalogSourceId)
            }
        }
    }
}
