import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int minCardWidth: 160
    readonly property int gridSpacing: MD.Token.spacing.medium
    readonly property int metaHeight: 64
    property string selectedSourceId: "freetp"

    signal openGame(string entryId)

    Component.onCompleted: Core.searchCatalog(selectedSourceId, "")

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentCol.implicitHeight + MD.Token.spacing.large
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: flick.width
            spacing: MD.Token.spacing.large

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
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
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                spacing: MD.Token.spacing.small

                Repeater {
                    model: Core.sources

                    MD.FilterChip {
                        required property string pluginId
                        required property string name

                        text: name
                        checked: root.selectedSourceId === pluginId
                        icon.name: pluginId === "online-fix"
                            ? MD.Token.icon.cloud_download
                            : MD.Token.icon.storefront
                        onClicked: {
                            root.selectedSourceId = pluginId
                            Core.searchCatalog(pluginId, "")
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large

                MD.Label {
                    Layout.fillWidth: true
                    text: Core.catalogLoading
                        ? qsTr("Загрузка каталога…")
                        : qsTr("Найдено: %1").arg(Core.catalog.rowCount())
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_large
                }

                MD.IconButton {
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.refresh
                    enabled: !Core.catalogLoading
                    onClicked: Core.refreshCatalog(root.selectedSourceId)
                }
            }

            MD.Label {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                visible: Core.catalogStatus.length > 0
                text: Core.catalogStatus
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_small
                wrapMode: Text.WordWrap
            }

            MD.LinearIndicator {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                visible: Core.catalogLoading
                indeterminate: true
            }

            Item {
                id: gridHost
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large

                property int columns: 3
                property real cardWidth: 160
                property real cardHeight: 280

                function relayout() {
                    const cols = Math.max(2, Math.floor((width + root.gridSpacing) / (root.minCardWidth + root.gridSpacing)))
                    const cardW = (width - root.gridSpacing * (cols - 1)) / cols
                    columns = cols
                    cardWidth = cardW
                    cardHeight = cardW * 4 / 3 + root.metaHeight
                    const rows = Math.max(1, Math.ceil(Core.catalog.rowCount() / cols))
                    Layout.preferredHeight = rows * (cardHeight + root.gridSpacing)
                }

                Timer {
                    id: layoutTimer
                    interval: 50
                    onTriggered: gridHost.relayout()
                }

                onWidthChanged: layoutTimer.restart()
                Component.onCompleted: relayout()

                Connections {
                    target: Core.catalog
                    function onModelReset() { gridHost.relayout() }
                    function onRowsInserted() { gridHost.relayout() }
                    function onRowsRemoved() { gridHost.relayout() }
                }

                GridView {
                    anchors.fill: parent
                    clip: true
                    interactive: false
                    model: Core.catalog
                    cellWidth: gridHost.cardWidth + root.gridSpacing
                    cellHeight: gridHost.cardHeight + root.gridSpacing
                    cacheBuffer: gridHost.cardHeight * 2
                    delegate: CatalogGameCard {
                        width: Math.max(0, gridHost.cardWidth - root.gridSpacing)
                        height: gridHost.cardHeight
                        onOpenDetails: function (id) { root.openGame(id) }
                    }
                }
            }
        }
    }
}
