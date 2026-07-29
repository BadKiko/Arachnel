import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Rectangle {
    id: root

    required property var page
    required property var prefs
    required property int pageMargin
    required property real barOpacity

    height: compactRow.implicitHeight + MD.Token.spacing.small * 2
    color: MD.Token.color.surface
    opacity: barOpacity
    visible: barOpacity > 0.01
    scale: 0.97 + 0.03 * barOpacity
    transformOrigin: Item.Top
    layer.enabled: visible

    Behavior on opacity {
        NumberAnimation {
            duration: MD.Token.duration.short4
            easing: MD.Token.easing.emphasized_decelerate
        }
    }

    transform: Translate {
        y: (1 - root.barOpacity) * -12
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        propagateComposedEvents: true
        onWheel: function (wheel) { wheel.accepted = false }
    }

    // Soft shadow so the bar feels like it settles over the grid.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.bottom
        height: 16
        opacity: root.barOpacity * 0.55
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: MD.Util.transparent(MD.Token.color.scrim, 0.18)
            }
            GradientStop {
                position: 1.0
                color: "transparent"
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: MD.Token.color.outline_variant
        opacity: 0.2 + 0.25 * root.barOpacity
    }

    RowLayout {
        id: compactRow
        anchors.fill: parent
        anchors.leftMargin: pageMargin + MD.Token.spacing.small
        anchors.rightMargin: pageMargin + MD.Token.spacing.small
        spacing: MD.Token.spacing.small

        MD.Label {
            text: qsTr("Catalog")
            typescale: MD.Token.typescale.title_large
            elide: Text.ElideRight
            maximumLineCount: 1
            scale: 0.94 + 0.06 * root.barOpacity
            transformOrigin: Item.Left
        }

        Item {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 20
            visible: Core.catalogLoading

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: MD.Util.transparent(MD.Token.color.primary, 0.12)
            }

            MD.LinearIndicator {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                implicitHeight: 4
                strokeWidth: implicitHeight
                indeterminate: true
                running: Core.catalogLoading && root.visible
                color: MD.Token.color.primary
                trackColor: MD.Util.transparent(color, 0.2)
            }
        }

        MD.Label {
            Layout.fillWidth: true
            text: Core.catalogLoading && Core.catalog.count === 0
                ? qsTr("Loading…")
                : qsTr("Found: %1").arg(Core.catalog.count)
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
            elide: Text.ElideRight
            maximumLineCount: 1
            opacity: 0.55 + 0.45 * root.barOpacity
        }

        Item {
            Layout.preferredWidth: compactFilterBtn.implicitWidth
            Layout.preferredHeight: compactFilterBtn.implicitHeight
            opacity: 0.4 + 0.6 * root.barOpacity

            MD.IconButton {
                id: compactFilterBtn
                anchors.centerIn: parent
                mdState.type: Core.catalogActiveFilterCount > 0
                              ? MD.Enum.IBtFilledTonal
                              : MD.Enum.IBtStandard
                icon.name: MD.Token.icon.filter_list
                onClicked: page.openFilterSheet()
            }

            Rectangle {
                visible: Core.catalogActiveFilterCount > 0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 2
                anchors.topMargin: 2
                width: Math.max(18, compactBadgeLabel.implicitWidth + 6)
                height: 18
                radius: 9
                color: MD.Token.color.error
                z: 2

                MD.Label {
                    id: compactBadgeLabel
                    anchors.centerIn: parent
                    text: Core.catalogActiveFilterCount
                    color: MD.Token.color.on_error
                    typescale: MD.Token.typescale.label_small
                }
            }
        }

        MD.IconButton {
            opacity: 0.4 + 0.6 * root.barOpacity
            mdState.type: page.listViewMode ? MD.Enum.IBtStandard : MD.Enum.IBtFilledTonal
            icon.name: MD.Token.icon.grid_view
            onClicked: prefs.viewMode = 0
        }

        MD.IconButton {
            opacity: 0.4 + 0.6 * root.barOpacity
            mdState.type: page.listViewMode ? MD.Enum.IBtFilledTonal : MD.Enum.IBtStandard
            icon.name: MD.Token.icon.view_list
            onClicked: prefs.viewMode = 1
        }

        MD.IconButton {
            opacity: 0.4 + 0.6 * root.barOpacity
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.refresh
            enabled: !Core.catalogLoading && Core.activeCatalogSourceIds.length > 0
            onClicked: Core.refreshSelectedCatalogs()
        }
    }
}
