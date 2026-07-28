import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var page
    spacing: MD.Token.spacing.medium

    signal browseAllRequested()
    signal helpMePickRequested()
    signal surpriseRequested()
    signal shortSessionRequested()
    signal friendsRequested()

    readonly property var recentGame: {
        const _n = Core.library.count
        return Core.library.mostRecentGame()
    }
    readonly property bool hasRecent: !!(recentGame && (recentGame.gameId || "").length)

    RowLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.small

        Item { Layout.fillWidth: true }

        MD.IconButton {
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.refresh
            enabled: !Core.catalogDiscovery.loading
            onClicked: Core.catalogDiscovery.refresh()
        }

        MD.Button {
            mdState.type: MD.Enum.BtText
            text: qsTr("All games")
            onClicked: root.browseAllRequested()
        }
    }

    // Jump back into something already installed.
    Rectangle {
        Layout.fillWidth: true
        visible: root.hasRecent
        radius: MD.Token.shape.corner.extra_large
        color: MD.Token.color.secondary_container
        implicitHeight: continueRow.implicitHeight + MD.Token.spacing.medium * 2

        RowLayout {
            id: continueRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: MD.Token.spacing.medium
            spacing: MD.Token.spacing.medium

            Rectangle {
                Layout.preferredWidth: 56
                Layout.preferredHeight: 74
                radius: MD.Token.shape.corner.medium
                color: MD.Token.color.surface_container_highest
                clip: true

                GamePoster {
                    anchors.fill: parent
                    source: root.recentGame.coverUrl || ""
                    seed: root.recentGame.title || ""
                    fallbackText: root.recentGame.title || "?"
                    cornerRadius: MD.Token.shape.corner.medium
                    decodeWidth: 112
                    decodeHeight: 148
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    text: qsTr("Jump back in")
                    color: MD.Token.color.on_secondary_container
                    typescale: MD.Token.typescale.label_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: root.recentGame.title || ""
                    color: MD.Token.color.on_secondary_container
                    typescale: MD.Token.typescale.title_medium
                    elide: Text.ElideRight
                }
            }

            MD.Button {
                mdState.type: MD.Enum.BtFilled
                text: qsTr("Continue")
                onClicked: page.openGame(root.recentGame.gameId)
            }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            cursorShape: Qt.PointingHandCursor
            onClicked: page.openGame(root.recentGame.gameId)
        }
    }

    // Equal-width quick actions.
    RowLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.small

        MD.Button {
            Layout.fillWidth: true
            mdState.type: MD.Enum.BtFilledTonal
            text: qsTr("Surprise me")
            onClicked: root.surpriseRequested()
        }

        MD.Button {
            Layout.fillWidth: true
            mdState.type: MD.Enum.BtFilledTonal
            text: qsTr("Help me pick")
            onClicked: root.helpMePickRequested()
        }

        MD.Button {
            Layout.fillWidth: true
            mdState.type: MD.Enum.BtFilledTonal
            text: qsTr("Short install")
            onClicked: root.shortSessionRequested()
        }
    }
}
