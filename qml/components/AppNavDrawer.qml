import QtQuick
import QtQuick.Layouts
import QtCore

import Arachnel.Core 1.0
import Qcm.Material as MD

// Launcher v2 navigation: an expressive expanded rail. Brand at the top, a search pill that jumps
// straight into the catalog, destinations with a pill that springs in, and settings at the
// bottom. It folds to an icon rail on narrow windows or when the user collapses it.
Rectangle {
    id: root

    property int currentIndex: 0
    property var model: []
    property int downloadBadge: 0
    property bool narrowWindow: false

    signal activated(int index)
    signal settingsRequested()
    signal searchRequested()

    Settings {
        id: prefs
        category: "navDrawer"
        property bool collapsed: false
    }

    readonly property bool expanded: !prefs.collapsed && !narrowWindow

    color: MD.Token.color.surface_container_low
    implicitWidth: expanded ? 256 : 88
    Behavior on implicitWidth {
        NumberAnimation { duration: AppMotion.scaled(420); easing: AppMotion.emphasized }
    }
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: MD.Token.spacing.medium
        anchors.bottomMargin: MD.Token.spacing.medium
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 4

        // ---------- Brand ----------
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            spacing: MD.Token.spacing.medium

            ShapeIcon {
                Layout.preferredWidth: 52
                Layout.preferredHeight: 52
                Layout.leftMargin: 6
                kind: "cookie9"
                color: MD.Token.color.primary
                iconColor: MD.Token.color.on_primary

                SpiderWebMark {
                    anchors.centerIn: parent
                    width: 28
                    height: 28
                    strokeColor: MD.Token.color.on_primary
                    strokeWidth: 1.6
                    rings: 3
                    spokes: 8
                }

                TapHandler { onTapped: root.activated(0) }
            }

            MD.Label {
                Layout.fillWidth: true
                visible: root.expanded
                opacity: root.expanded ? 1 : 0
                text: "Arachnel"
                typescale: MD.Token.typescale.title_large
                font.weight: Font.Bold
                Behavior on opacity { NumberAnimation { duration: AppMotion.short } }
            }

            MD.IconButton {
                visible: root.expanded
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.menu_open
                Accessible.name: qsTr("Collapse")
                onClicked: prefs.collapsed = true
            }
        }

        MD.IconButton {
            visible: !root.expanded && !root.narrowWindow
            Layout.alignment: Qt.AlignHCenter
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.menu
            Accessible.name: qsTr("Expand")
            onClicked: prefs.collapsed = false
        }

        // ---------- Search pill ----------
        Rectangle {
            id: searchPill
            Layout.fillWidth: true
            Layout.topMargin: MD.Token.spacing.small
            Layout.bottomMargin: MD.Token.spacing.small
            Layout.preferredHeight: 56
            radius: searchTap.pressed ? 16 : 28
            color: searchHover.hovered ? MD.Token.color.surface_container_highest
                                       : MD.Token.color.surface_container_high
            Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
            Behavior on color { ColorAnimation { duration: AppMotion.short } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.expanded ? 18 : 0
                spacing: MD.Token.spacing.medium

                MD.Icon {
                    Layout.alignment: root.expanded ? Qt.AlignVCenter : Qt.AlignCenter
                    Layout.fillWidth: !root.expanded
                    horizontalAlignment: Text.AlignHCenter
                    name: MD.Token.icon.search
                    size: 24
                    color: MD.Token.color.on_surface_variant
                }
                MD.Label {
                    Layout.fillWidth: true
                    visible: root.expanded
                    text: qsTr("Search")
                    typescale: MD.Token.typescale.body_large
                    color: MD.Token.color.on_surface_variant
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    maximumLineCount: 1
                }
                MD.Label {
                    visible: root.expanded
                    Layout.rightMargin: 16
                    wrapMode: Text.NoWrap
                    text: "Ctrl K"
                    typescale: MD.Token.typescale.label_medium
                    color: MD.Token.color.outline
                }
            }

            HoverHandler { id: searchHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { id: searchTap; onTapped: root.searchRequested() }
        }

        // ---------- Destinations ----------
        Repeater {
            model: root.model

            delegate: Item {
                id: entry
                required property int index
                required property var modelData
                readonly property bool selected: index === root.currentIndex
                readonly property bool hovered: entryHover.hovered

                Layout.fillWidth: true
                Layout.preferredHeight: root.expanded ? 56 : 64

                Accessible.role: Accessible.Button
                Accessible.name: modelData.name

                // Hover: a fixed-size state layer that only fades. It used to be the same pill as the
                // selection one, so every pass of the pointer made it spring wider and back.
                Rectangle {
                    id: hoverLayer
                    y: pill.y
                    x: root.expanded ? 0 : (parent.width - width) / 2
                    width: root.expanded ? parent.width : 56
                    height: pill.height
                    radius: height / 2
                    color: MD.Util.transparent(MD.Token.color.on_surface, 0.08)
                    opacity: entry.hovered && !entry.selected ? 1 : 0
                    scale: entryTap.pressed ? 0.96 : 1
                    Behavior on opacity { NumberAnimation { duration: AppMotion.short } }
                    Behavior on scale { NumberAnimation { duration: AppMotion.fast } }
                }

                // Selection: expanded, a full-width pill growing from the left; collapsed, a
                // 56 x 32 pill under the icon, like the rail. Only this one springs.
                Rectangle {
                    id: pill
                    y: root.expanded ? (parent.height - height) / 2 : 6
                    x: root.expanded ? 0 : (parent.width - width) / 2
                    height: root.expanded ? 56 : 32
                    width: root.expanded
                           ? (entry.selected ? parent.width : parent.width * 0.4)
                           : (entry.selected ? 56 : 32)
                    radius: height / 2
                    color: entry.selected ? MD.Token.color.secondary_container : "transparent"
                    scale: entryTap.pressed ? 0.96 : 1

                    Behavior on width {
                        NumberAnimation { duration: AppMotion.scaled(420); easing.type: Easing.OutBack; easing.overshoot: 1.2 }
                    }
                    Behavior on color { ColorAnimation { duration: AppMotion.short } }
                    Behavior on scale { NumberAnimation { duration: AppMotion.fast } }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.expanded ? 18 : 0
                    anchors.rightMargin: root.expanded ? 16 : 0
                    spacing: MD.Token.spacing.medium
                    visible: root.expanded

                    MD.Icon {
                        name: entry.modelData.icon
                        fill: entry.selected
                        size: 24
                        color: entry.selected ? MD.Token.color.on_secondary_container
                                              : MD.Token.color.on_surface_variant
                    }
                    MD.Label {
                        Layout.fillWidth: true
                        text: entry.modelData.name
                        typescale: MD.Token.typescale.label_large
                        font.pixelSize: 15
                        font.weight: entry.selected ? Font.Bold : Font.Medium
                        color: entry.selected ? MD.Token.color.on_secondary_container
                                              : MD.Token.color.on_surface_variant
                    }
                    Rectangle {
                        visible: entry.index === 5 && root.downloadBadge > 0
                        Layout.preferredHeight: 22
                        Layout.preferredWidth: Math.max(22, badgeText.implicitWidth + 12)
                        radius: 11
                        color: MD.Token.color.primary
                        MD.Label {
                            id: badgeText
                            anchors.centerIn: parent
                            text: root.downloadBadge
                            typescale: MD.Token.typescale.label_medium
                            color: MD.Token.color.on_primary
                        }
                    }
                }

                // Collapsed layout: the icon centred in the pill, a short label under it.
                MD.Icon {
                    anchors.centerIn: pill
                    visible: !root.expanded
                    name: entry.modelData.icon
                    fill: entry.selected
                    size: 24
                    color: entry.selected ? MD.Token.color.on_secondary_container
                                          : MD.Token.color.on_surface_variant
                }
                Item {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: pill.bottom
                    anchors.topMargin: 4
                    height: collapsedLabel.implicitHeight
                    visible: !root.expanded

                    MD.Label {
                        id: collapsedLabel
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: entry.modelData.name
                        typescale: MD.Token.typescale.label_small
                        color: entry.selected ? MD.Token.color.on_surface : MD.Token.color.on_surface_variant
                        width: 76
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                }

                HoverHandler { id: entryHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { id: entryTap; onTapped: root.activated(entry.index) }

                // The collapsed item already carries its name; the tooltip is only for names that
                // had to be elided. It sits beside the item: above it, it covered the previous
                // entry, which then lost hover as the pointer moved up, and the pill and tooltip
                // flickered.
                MD.ToolTip {
                    visible: !root.expanded && entry.hovered && collapsedLabel.truncated
                    delay: 500
                    x: parent.width + 8
                    y: (parent.height - height) / 2
                    text: entry.modelData.name
                }
            }
        }

        Item { Layout.fillHeight: true }

        // ---------- Settings ----------
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 56

            readonly property bool selected: root.currentIndex === 6

            Rectangle {
                anchors.fill: parent
                radius: 28
                color: parent.selected ? MD.Token.color.secondary_container
                     : MD.Util.transparent(MD.Token.color.on_surface, settingsHover.hovered ? 0.08 : 0)
                Behavior on color { ColorAnimation { duration: AppMotion.short } }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.expanded ? 18 : 0
                spacing: MD.Token.spacing.medium

                MD.Icon {
                    Layout.fillWidth: !root.expanded
                    horizontalAlignment: Text.AlignHCenter
                    name: MD.Token.icon.settings
                    size: 24
                    color: MD.Token.color.on_surface_variant
                    rotation: settingsHover.hovered ? 60 : 0
                    Behavior on rotation { NumberAnimation { duration: AppMotion.scaled(400); easing.type: Easing.OutCubic } }
                }
                MD.Label {
                    Layout.fillWidth: true
                    visible: root.expanded
                    text: qsTr("Settings")
                    typescale: MD.Token.typescale.label_large
                    font.pixelSize: 15
                    color: MD.Token.color.on_surface_variant
                }
            }

            HoverHandler { id: settingsHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.settingsRequested() }
        }
    }
}
