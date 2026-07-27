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

    readonly property bool shelvesEmpty:
        Core.catalogDiscovery.onFireShelf.count === 0
        && Core.catalogDiscovery.friendsShelf.count === 0
        && Core.catalogDiscovery.newShelf.count === 0
    readonly property bool showSkeleton: Core.catalogDiscovery.loading
                                         || (Core.catalogLoading && shelvesEmpty)
    readonly property bool noFeed: !Core.catalogDiscovery.feedLoaded
                                   && !Core.catalogDiscovery.loading
                                   && !Core.catalogLoading
    readonly property bool emptyShelves: Core.catalogDiscovery.feedLoaded
                                         && shelvesEmpty
                                         && !Core.catalogDiscovery.loading
                                         && !Core.catalogLoading

    readonly property var recentGame: {
        const _n = Core.library.count
        return Core.library.mostRecentGame()
    }
    readonly property bool hasRecent: !!(recentGame && (recentGame.gameId || "").length)

    RowLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.medium

        ColumnLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.extra_small

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Don't know what to play?")
                typescale: MD.Token.typescale.headline_medium
            }

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Answer in one tap — or scroll the shelves below.")
                wrapMode: Text.WordWrap
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
            }
        }

        MD.IconButton {
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.refresh
            enabled: !Core.catalogDiscovery.loading
            onClicked: Core.catalogDiscovery.refresh()
        }

        MD.Button {
            mdState.type: MD.Enum.BtOutlined
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

    // Decision tiles — the core of "what to play".
    GridLayout {
        Layout.fillWidth: true
        columns: width >= 900 ? 4 : 2
        rowSpacing: MD.Token.spacing.small
        columnSpacing: MD.Token.spacing.small

        Repeater {
            model: [
                {
                    title: qsTr("Surprise me"),
                    hint: qsTr("Open a random hit"),
                    icon: MD.Token.icon.star,
                    action: "surprise"
                },
                {
                    title: qsTr("Help me pick"),
                    hint: qsTr("3 quick questions"),
                    icon: MD.Token.icon.auto_awesome,
                    action: "pick"
                },
                {
                    title: qsTr("Short install"),
                    hint: qsTr("About 1–5 GB"),
                    icon: MD.Token.icon.hard_drive,
                    action: "short"
                },
                {
                    title: qsTr("With friends"),
                    hint: qsTr("Co-op & party"),
                    icon: MD.Token.icon.groups,
                    action: "friends"
                }
            ]

            Rectangle {
                id: actionTile
                required property var modelData

                Layout.fillWidth: true
                Layout.preferredHeight: 88
                radius: MD.Token.shape.corner.extra_large
                color: MD.Token.color.surface_container_high
                border.width: 1
                border.color: MD.Util.transparent(MD.Token.color.outline_variant, 0.7)

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: MD.Token.spacing.medium
                    spacing: MD.Token.spacing.medium

                    Rectangle {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        radius: MD.Token.shape.corner.large
                        color: MD.Token.color.primary_container

                        MD.Icon {
                            anchors.centerIn: parent
                            name: actionTile.modelData.icon
                            size: 24
                            color: MD.Token.color.on_primary_container
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        MD.Label {
                            Layout.fillWidth: true
                            text: actionTile.modelData.title
                            typescale: MD.Token.typescale.title_small
                            elide: Text.ElideRight
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: actionTile.modelData.hint
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_small
                            elide: Text.ElideRight
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const a = actionTile.modelData.action
                        if (a === "surprise")
                            root.surpriseRequested()
                        else if (a === "pick")
                            root.helpMePickRequested()
                        else if (a === "short")
                            root.shortSessionRequested()
                        else if (a === "friends")
                            root.friendsRequested()
                    }
                }
            }
        }
    }

    CatalogDiscoveryMoodRow {
        Layout.fillWidth: true
        page: root.page
    }
}
