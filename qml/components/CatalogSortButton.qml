import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

// Expressive sort control: a labelled pill that shows the active key and direction, opening a
// popover with the sort key list and a connected direction toggle. Each key maps to a pair of
// catalog sort modes (primary / reverse).
Item {
    id: root

    // Kept for call-site compatibility; the keys below are the single source of truth.
    property var sortOptions: []

    signal sortModeChosen(int mode)

    readonly property var keys: [
        { icon: MD.Token.icon.schedule, label: qsTr("Date added"), modes: [0, 1],
          directions: [qsTr("Newest"), qsTr("Oldest")] },
        { icon: MD.Token.icon.sort_by_alpha, label: qsTr("Title"), modes: [2, 3],
          directions: [qsTr("A → Z"), qsTr("Z → A")] },
        { icon: MD.Token.icon.hard_drive, label: qsTr("Size"), modes: [6, 7],
          directions: [qsTr("Largest"), qsTr("Smallest")] },
        { icon: MD.Token.icon.package_2, label: qsTr("Type"), modes: [4, 5],
          directions: [qsTr("Portable"), qsTr("Installer")] }
    ]

    readonly property int mode: Core.catalog.sortMode
    readonly property int keyIndex: {
        for (let i = 0; i < keys.length; ++i) {
            if (keys[i].modes.indexOf(mode) >= 0)
                return i
        }
        return 0
    }
    readonly property int dirIndex: Math.max(0, keys[keyIndex].modes.indexOf(mode))
    // The arrow only hints at order for keys where "first" reads as down / up.
    readonly property bool reversed: dirIndex === 1

    function choose(mode) {
        Core.catalog.sortMode = mode
        root.sortModeChosen(mode)
    }

    implicitWidth: pill.implicitWidth
    implicitHeight: 40

    Rectangle {
        id: pill
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: pillRow.implicitWidth + 28
        width: implicitWidth
        height: 40
        radius: pillTap.pressed ? 12 : (popup.visible ? 14 : height / 2)
        color: popup.visible ? MD.Token.color.secondary_container
             : (pillHover.hovered ? MD.Token.color.surface_container_highest
                                  : MD.Token.color.surface_container_high)

        Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: AppMotion.short } }

        Row {
            id: pillRow
            anchors.centerIn: parent
            spacing: 8

            MD.Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: root.keys[root.keyIndex].icon
                size: 20
                color: popup.visible ? MD.Token.color.on_secondary_container
                                     : MD.Token.color.on_surface_variant
            }
            MD.Label {
                anchors.verticalCenter: parent.verticalCenter
                text: root.keys[root.keyIndex].label
                typescale: MD.Token.typescale.label_large
                color: popup.visible ? MD.Token.color.on_secondary_container
                                     : MD.Token.color.on_surface
            }
            MD.Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: root.reversed ? MD.Token.icon.arrow_upward : MD.Token.icon.arrow_downward
                size: 18
                color: popup.visible ? MD.Token.color.on_secondary_container
                                     : MD.Token.color.on_surface_variant
            }
        }

        HoverHandler { id: pillHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { id: pillTap; onTapped: popup.visible ? popup.close() : popup.open() }

        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Sort: %1").arg(root.keys[root.keyIndex].label)
    }

    Popup {
        id: popup
        parent: pill
        y: pill.height + 6
        x: pill.width - width
        width: 280
        padding: 8
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppMotion.short }
                NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: AppMotion.medium; easing.type: Easing.OutBack }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: AppMotion.short }
        }
        transformOrigin: Popup.TopRight

        background: Rectangle {
            radius: MD.Token.shape.corner.extra_large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
        }

        contentItem: ColumnLayout {
            spacing: 2

            MD.Label {
                Layout.leftMargin: 12
                Layout.topMargin: 8
                Layout.bottomMargin: 6
                text: qsTr("Sort by")
                typescale: MD.Token.typescale.label_large
                color: MD.Token.color.primary
            }

            Repeater {
                model: root.keys

                delegate: Rectangle {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool chosen: root.keyIndex === index

                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    radius: rowTap.pressed ? 12 : 24
                    color: chosen ? MD.Token.color.secondary_container
                         : (rowHover.hovered ? MD.Token.color.surface_container_highest : "transparent")
                    Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
                    Behavior on color { ColorAnimation { duration: AppMotion.short } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        MD.Icon {
                            name: row.modelData.icon
                            size: 22
                            color: row.chosen ? MD.Token.color.on_secondary_container
                                              : MD.Token.color.on_surface_variant
                        }
                        MD.Label {
                            Layout.fillWidth: true
                            text: row.modelData.label
                            typescale: MD.Token.typescale.body_large
                            color: row.chosen ? MD.Token.color.on_secondary_container
                                              : MD.Token.color.on_surface
                        }
                        MD.Icon {
                            visible: row.chosen
                            name: MD.Token.icon.check
                            size: 20
                            color: MD.Token.color.on_secondary_container
                        }
                    }

                    HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        id: rowTap
                        // Switching key keeps the chosen direction.
                        onTapped: root.choose(row.modelData.modes[root.dirIndex])
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 6
                Layout.bottomMargin: 6
                Layout.preferredHeight: 1
                color: MD.Token.color.outline_variant
            }

            ConnectedTextToggle {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 6
                labels: root.keys[root.keyIndex].directions
                currentIndex: root.dirIndex
                onPicked: function (index) { root.choose(root.keys[root.keyIndex].modes[index]) }
            }
        }
    }
}
