import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int minCardWidth: 160
    readonly property int gridSpacing: MD.Token.spacing.medium
    readonly property int metaHeight: 48

    property string selectedSourceId: "freetp"
    property var featuredGame: Core.library.gameAt(0)

    signal openGame(string gameId)

    Component.onCompleted: Core.searchCatalog(selectedSourceId, "")

    Connections {
        target: Core.library
        function onModelReset() { root.featuredGame = Core.library.gameAt(0) }
        function onDataChanged() { root.featuredGame = Core.library.gameAt(0) }
    }

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

            MD.ElevationRectangle {
                id: heroCard
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.topMargin: MD.Token.spacing.medium
                Layout.preferredHeight: 280
                radius: MD.Token.shape.corner.extra_large
                color: MD.Token.color.surface_container
                elevation: MD.Token.elevation.level1
                clip: true

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: MD.Token.spacing.large
                    spacing: MD.Token.spacing.large

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: MD.Token.spacing.small

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Недавно играли")
                            color: MD.Token.color.primary
                            typescale: MD.Token.typescale.label_large
                            elide: Text.ElideRight
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: root.featuredGame.title ?? qsTr("Нет игр")
                            typescale: MD.Token.typescale.headline_large
                            elide: Text.ElideRight
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: (root.featuredGame.sourceName ?? "") + " · v" + (root.featuredGame.version ?? "")
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_large
                            elide: Text.ElideRight
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small

                            MD.Button {
                                text: qsTr("Играть")
                                icon.name: MD.Token.icon.play_arrow
                                mdState.type: MD.Enum.BtFilled
                                enabled: !!(root.featuredGame.gameId)
                                onClicked: Core.launchGame(root.featuredGame.gameId)
                            }

                            MD.Button {
                                text: qsTr("Подробнее")
                                icon.name: MD.Token.icon.info
                                mdState.type: MD.Enum.BtOutlined
                                enabled: !!(root.featuredGame.gameId)
                                onClicked: root.openGame(root.featuredGame.gameId)
                            }

                            MD.Button {
                                text: qsTr("Обновления")
                                icon.name: MD.Token.icon.update
                                mdState.type: MD.Enum.BtText
                                onClicked: Core.checkUpdates()
                            }
                        }
                    }

                    GamePoster {
                        Layout.preferredWidth: 140
                        Layout.preferredHeight: 186
                        Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                        source: root.featuredGame.coverUrl ?? ""
                        fallbackText: (root.featuredGame.title ?? "?").charAt(0)
                        cornerRadius: MD.Token.shape.corner.extra_large
                        onClicked: {
                            if (root.featuredGame.gameId)
                                root.openGame(root.featuredGame.gameId)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                spacing: MD.Token.spacing.medium

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("В библиотеке")
                    value: String(Core.library.rowCount())
                    iconName: MD.Token.icon.sports_esports
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Источники")
                    value: String(Core.sources.rowCount())
                    iconName: MD.Token.icon.storefront
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Задачи")
                    value: String(Core.jobs.rowCount())
                    iconName: MD.Token.icon.downloading
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Обновления")
                    value: String(Core.library.updateCount())
                    iconName: MD.Token.icon.update
                }
            }

            MD.Pane {
                id: jobsPane
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                visible: Core.jobs.rowCount() > 0
                padding: MD.Token.spacing.medium
                radius: MD.Token.shape.corner.extra_large
                backgroundColor: MD.Token.color.secondary_container
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Активные задачи")
                        color: MD.Token.color.on_secondary_container
                        typescale: MD.Token.typescale.title_small
                        elide: Text.ElideRight
                    }

                    Repeater {
                        model: Core.jobs

                        DownloadJobCard {
                            required property string jobId
                            required property string title
                            required property string kindLabel
                            required property string status
                            required property int progress
                            required property string detail
                            Layout.fillWidth: true
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
                    text: qsTr("Моя библиотека")
                    typescale: MD.Token.typescale.title_large
                }

                MD.Label {
                    text: qsTr("%1 игр").arg(Core.library.rowCount())
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_large
                }
            }

            Item {
                id: gridHost
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large

                // Debounce layout math while resizing
                property int columns: 3
                property real cardWidth: 160
                property real cardHeight: 260

                function relayout() {
                    const cols = Math.max(2, Math.floor((width + root.gridSpacing) / (root.minCardWidth + root.gridSpacing)))
                    const cardW = (width - root.gridSpacing * (cols - 1)) / cols
                    columns = cols
                    cardWidth = cardW
                    cardHeight = cardW * 4 / 3 + root.metaHeight
                    const rows = Math.max(1, Math.ceil(Core.library.rowCount() / cols))
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
                    target: Core.library
                    function onModelReset() { gridHost.relayout() }
                    function onRowsInserted() { gridHost.relayout() }
                    function onRowsRemoved() { gridHost.relayout() }
                }

                GridView {
                    anchors.fill: parent
                    clip: true
                    interactive: false
                    model: Core.library
                    cellWidth: gridHost.cardWidth + root.gridSpacing
                    cellHeight: gridHost.cardHeight + root.gridSpacing
                    cacheBuffer: 0
                    delegate: LibraryGameCard {
                        width: Math.max(0, gridHost.cardWidth - root.gridSpacing)
                        height: gridHost.cardHeight
                        onOpenDetails: function (id) { root.openGame(id) }
                    }
                }
            }
        }
    }
}
