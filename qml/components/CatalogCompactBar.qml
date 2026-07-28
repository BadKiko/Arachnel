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
    visible: barOpacity > 0.02
    layer.enabled: visible

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        propagateComposedEvents: true
        onWheel: function (wheel) { wheel.accepted = false }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: MD.Token.color.outline_variant
        opacity: 0.35
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
        }

        MD.CircularIndicator {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            visible: Core.catalogLoading
            indeterminate: true
            running: Core.catalogLoading
            strokeWidth: 3
        }

        MD.Label {
            Layout.fillWidth: true
            text: Core.catalogLoading
                ? qsTr("Loading…")
                : qsTr("Found: %1").arg(Core.catalog.count)
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        MD.Badge {
            count: Core.catalogActiveFilterCount
            backgroundColor: MD.Token.color.error
            textColor: MD.Token.color.on_error

            MD.IconButton {
                mdState.type: Core.catalogActiveFilterCount > 0
                              ? MD.Enum.IBtFilledTonal
                              : MD.Enum.IBtStandard
                icon.name: MD.Token.icon.filter_list
                onClicked: page.openFilterSheet()
            }
        }

        MD.IconButton {
            mdState.type: page.listViewMode ? MD.Enum.IBtStandard : MD.Enum.IBtFilledTonal
            icon.name: MD.Token.icon.grid_view
            onClicked: prefs.viewMode = 0
        }

        MD.IconButton {
            mdState.type: page.listViewMode ? MD.Enum.IBtFilledTonal : MD.Enum.IBtStandard
            icon.name: MD.Token.icon.view_list
            onClicked: prefs.viewMode = 1
        }

        MD.IconButton {
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.refresh
            enabled: !Core.catalogLoading && Core.activeCatalogSourceIds.length > 0
            onClicked: Core.refreshSelectedCatalogs()
        }
    }
}
