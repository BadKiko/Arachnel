import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property int contentWidth
    required property bool hasSelection
    required property bool listViewMode

    signal filterRequested()
    signal viewModeChangeRequested(int mode)
    signal refreshRequested()

    width: contentWidth
    height: col.implicitHeight

    ColumnLayout {
        id: col
        width: root.contentWidth
        spacing: MD.Token.spacing.small

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: MD.Token.spacing.small
            spacing: MD.Token.spacing.small

            MD.CircularIndicator {
                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                visible: Core.catalogLoading
                indeterminate: true
                running: Core.catalogLoading
                strokeWidth: 3
            }

            MD.Label {
                Layout.fillWidth: true
                text: Core.catalogLoading
                    ? qsTr("Loading catalog…")
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
                    onClicked: root.filterRequested()
                }
            }

            MD.IconButton {
                mdState.type: root.listViewMode ? MD.Enum.IBtStandard : MD.Enum.IBtFilledTonal
                icon.name: MD.Token.icon.grid_view
                onClicked: root.viewModeChangeRequested(0)
            }

            MD.IconButton {
                mdState.type: root.listViewMode ? MD.Enum.IBtFilledTonal : MD.Enum.IBtStandard
                icon.name: MD.Token.icon.view_list
                onClicked: root.viewModeChangeRequested(1)
            }

            MD.IconButton {
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.refresh
                enabled: !Core.catalogLoading && root.hasSelection
                onClicked: root.refreshRequested()
            }
        }

        MD.Label {
            Layout.fillWidth: true
            visible: Core.catalogStatus.length > 0
            text: Core.catalogStatus
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Item {
            Layout.preferredHeight: MD.Token.spacing.extra_small
        }
    }
}
