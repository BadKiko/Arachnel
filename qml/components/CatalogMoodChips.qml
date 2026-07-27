import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var page

    signal whatToPlayRequested()

    readonly property var moods: [
        { id: "", label: qsTr("All shelves"), icon: MD.Token.icon.auto_awesome, hint: qsTr("Hits, new, friends, solo") },
        { id: "solo", label: qsTr("Solo"), icon: MD.Token.icon.person, hint: qsTr("Single-player") },
        { id: "friends", label: qsTr("With friends"), icon: MD.Token.icon.groups, hint: qsTr("Co-op & party") },
        { id: "new", label: qsTr("New games"), icon: MD.Token.icon.new_releases, hint: qsTr("Recently added") }
    ]

    readonly property var moodLabels: ({
        "": qsTr("All shelves"),
        "solo": qsTr("Solo"),
        "friends": qsTr("With friends"),
        "new": qsTr("New games")
    })

    readonly property string activeMoodLabel: moodLabels[Core.catalogDiscovery.moodId || ""] || ""

    ColumnLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.extra_small

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Filter discovery")
            typescale: MD.Token.typescale.title_medium
        }

        MD.Label {
            Layout.fillWidth: true
            text: Core.catalogDiscovery.moodActive
                  ? qsTr("Showing matches only — pick All shelves to see every category again.")
                  : qsTr("Tap a filter to narrow the list, or use Help me pick for a wizard.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.small
        visible: Core.catalogDiscovery.moodActive

        MD.FilterChip {
            text: activeMoodLabel
            checked: true
            onClicked: Core.catalogDiscovery.moodId = ""
        }

        MD.Button {
            mdState.type: MD.Enum.BtText
            text: qsTr("Clear filter")
            onClicked: Core.catalogDiscovery.moodId = ""
        }

        Item { Layout.fillWidth: true }

        MD.Label {
            text: Core.catalogDiscovery.onFireShelf.count + " " + qsTr("games")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.small
        visible: !Core.catalogDiscovery.moodActive

        Item { Layout.fillWidth: true }

        MD.Button {
            mdState.type: MD.Enum.BtText
            text: qsTr("Help me pick")
            onClicked: root.whatToPlayRequested()
        }
    }

    Flickable {
        Layout.fillWidth: true
        Layout.preferredHeight: 92
        visible: !Core.catalogDiscovery.moodActive
        contentWidth: moodRow.width
        contentHeight: height
        clip: true
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds

        Row {
            id: moodRow
            spacing: MD.Token.spacing.small
            height: parent.height

            Repeater {
                model: root.moods

                Item {
                    id: moodTile
                    required property var modelData
                    width: 132
                    height: moodRow.height

                    readonly property bool selected: (Core.catalogDiscovery.moodId || "") === modelData.id

                    Rectangle {
                        anchors.fill: parent
                        radius: MD.Token.shape.corner.large
                        color: moodTile.selected
                               ? MD.Token.color.secondary_container
                               : MD.Token.color.surface_container_high
                        border.width: moodTile.selected ? 0 : 1
                        border.color: MD.Util.transparent(MD.Token.color.outline, 0.35)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: MD.Token.spacing.medium
                        spacing: 4

                        MD.Icon {
                            name: modelData.icon
                            size: 22
                            color: moodTile.selected
                                   ? MD.Token.color.on_secondary_container
                                   : MD.Token.color.on_surface_variant
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: modelData.label
                            color: moodTile.selected
                                   ? MD.Token.color.on_secondary_container
                                   : MD.Token.color.on_surface
                            typescale: MD.Token.typescale.title_small
                            elide: Text.ElideRight
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: modelData.hint
                            color: moodTile.selected
                                   ? MD.Util.transparent(MD.Token.color.on_secondary_container, 0.75)
                                   : MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.label_small
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Core.catalogDiscovery.moodId = modelData.id
                    }
                }
            }
        }
    }
}
