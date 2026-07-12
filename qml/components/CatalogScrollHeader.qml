import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property int contentWidth
    required property string selectedSourceId
    required property bool listViewMode

    signal sourceSelected(string pluginId)
    signal sortRequested(var anchor)
    signal viewModeChangeRequested(int mode)
    signal refreshRequested()

    width: contentWidth
    height: col.implicitHeight

    ColumnLayout {
        id: col
        width: root.contentWidth
        spacing: MD.Token.spacing.small

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: MD.Token.spacing.medium
            spacing: 4

            MD.Label {
                text: qsTr("Каталог")
                typescale: MD.Token.typescale.headline_medium
            }

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Источник определяет способ установки — у каждого плагина свой пайплайн.")
                wrapMode: Text.WordWrap
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            Repeater {
                model: Core.sources

                MD.FilterChip {
                    required property string pluginId
                    required property string name
                    required property bool sourceEnabled

                    visible: sourceEnabled
                    text: name
                    checked: root.selectedSourceId === pluginId
                    onClicked: root.sourceSelected(pluginId)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                text: Core.catalogLoading
                    ? qsTr("Загрузка каталога…")
                    : qsTr("Найдено: %1").arg(Core.catalog.count)
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_large
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            MD.IconButton {
                id: sortBtn
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.sort
                onClicked: root.sortRequested(sortBtn)
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
                enabled: !Core.catalogLoading && root.selectedSourceId.length > 0
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

        MD.LinearIndicator {
            Layout.fillWidth: true
            visible: Core.catalogLoading
            indeterminate: true
        }

        Item {
            Layout.preferredHeight: MD.Token.spacing.extra_small
        }
    }
}
