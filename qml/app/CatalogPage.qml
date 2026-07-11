import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int cellWidth: 176
    readonly property int cellHeight: 284
    readonly property int cardWidth: 160
    readonly property int cardHeight: 268
    readonly property int pageMargin: MD.Token.spacing.large
    readonly property int cardRadius: MD.Token.shape.corner.extra_large
    readonly property bool noSources: Core.sources.enabledCount === 0
    readonly property bool catalogEmpty: !Core.catalogLoading && Core.catalog.count === 0

    property string selectedSourceId: Core.sources.firstEnabledId

    signal openGame(string entryId)
    signal openSettings()
    signal addSourceRequested()

    function ensureValidSource() {
        if (Core.sources.isSourceEnabled(root.selectedSourceId))
            return
        const first = Core.sources.firstEnabledId
        if (!first.length) {
            root.selectedSourceId = ""
            return
        }
        root.selectedSourceId = first
        Core.searchCatalog(first, "")
    }

    Component.onCompleted: {
        if (selectedSourceId.length)
            Core.searchCatalog(selectedSourceId, "")
    }

    Connections {
        target: Core.sources
        function onSourcesChanged() {
            root.ensureValidSource()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: MD.Token.spacing.medium
        visible: !root.noSources

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
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
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
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
                    onClicked: {
                        root.selectedSourceId = pluginId
                        Core.searchCatalog(pluginId, "")
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin

            MD.Label {
                Layout.fillWidth: true
                text: Core.catalogLoading
                    ? qsTr("Загрузка каталога…")
                    : qsTr("Найдено: %1").arg(Core.catalog.count)
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_large
            }

            MD.IconButton {
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.refresh
                enabled: !Core.catalogLoading && root.selectedSourceId.length > 0
                onClicked: Core.refreshCatalog(root.selectedSourceId)
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
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
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
            visible: Core.catalogLoading
            indeterminate: true
        }

        // Empty results (source selected, but nothing found)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            visible: root.catalogEmpty && !Core.catalogLoading

            Item {
                id: emptyResultsCard
                anchors.centerIn: parent
                width: Math.min(parent.width, 720)
                height: 220
                clip: true

                layer.enabled: true
                layer.effect: MD.RoundClip {
                    corners: MD.Util.corners(root.cardRadius)
                    size: Qt.vector2d(emptyResultsCard.width, emptyResultsCard.height)
                }

                Rectangle {
                    anchors.fill: parent
                    color: MD.Token.color.surface_container_high
                }

                SpiderWebMark {
                    width: 260
                    height: 260
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: -130
                    strokeColor: MD.Token.color.primary
                    strokeWidth: 2.5
                    opacity: 0.18
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: MD.Token.spacing.extra_large
                    anchors.rightMargin: MD.Token.spacing.extra_large + 80
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Ничего не найдено")
                        typescale: MD.Token.typescale.title_large
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Попробуйте другой запрос или обновите каталог.")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                        wrapMode: Text.WordWrap
                    }

                    MD.Button {
                        text: qsTr("Обновить")
                        icon.name: MD.Token.icon.refresh
            mdState.type: MD.Enum.BtFilledTonal
                        onClicked: Core.refreshCatalog(root.selectedSourceId)
                    }
                }
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            visible: !root.catalogEmpty || Core.catalogLoading
            clip: true
            model: Core.catalog
            cellWidth: root.cellWidth
            cellHeight: root.cellHeight
            cacheBuffer: root.cellHeight * 2
            reuseItems: false
            boundsBehavior: Flickable.StopAtBounds
            pixelAligned: true

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: CatalogGameCard {
                width: root.cardWidth
                height: root.cardHeight
                onOpenDetails: function (id) { root.openGame(id) }
            }
        }
    }

    // ── No sources / empty catalog ───────────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: root.noSources

        ColumnLayout {
            anchors.centerIn: parent
            spacing: MD.Token.spacing.medium
            width: Math.min(parent.width - pageMargin * 2, 420)

            SpiderWebMark {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 160
                Layout.preferredHeight: 160
                width: 160
                height: 160
                strokeColor: MD.Token.color.primary
                strokeWidth: 2.5
                opacity: 0.35
            }

            MD.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Нет игр")
                typescale: MD.Token.typescale.title_large
            }

            MD.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Добавьте источник в настройках — и каталог заполнится.")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: MD.Token.spacing.small

                MD.Button {
                    text: qsTr("Добавить источник")
                    icon.name: MD.Token.icon.add
                    mdState.type: MD.Enum.BtFilled
                    onClicked: root.addSourceRequested()
                }

                MD.Button {
                    text: qsTr("Настройки")
                    icon.name: MD.Token.icon.settings
                    mdState.type: MD.Enum.BtOutlined
                    onClicked: root.openSettings()
                }
            }
        }
    }
}
