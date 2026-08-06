import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int pageMargin: MD.Token.spacing.large
    readonly property int gridSpacing: MD.Token.spacing.medium
    readonly property int minCardWidth: 140
    readonly property int metaHeight: 48
    readonly property bool favoritesEmpty: favoritesModel.count === 0

    signal openGame(string gameId)

    ListModel {
        id: favoritesModel
    }

    function refreshFavorites() {
        const ids = Core.settings.bookmarkedEntryIds || []
        favoritesModel.clear()
        for (let i = 0; i < ids.length; ++i) {
            const id = String(ids[i] || "")
            if (!id.length)
                continue
            const info = Core.entryDetails(id)
            favoritesModel.append({
                                      gameId: id,
                                      title: String(info.title || id),
                                      coverUrl: String(info.coverUrl || ""),
                                      sourceName: String(info.sourceName || info.sourceId || "")
                                  })
        }
    }

    Connections {
        target: Core.settings
        function onBookmarkedEntryIdsChanged() { root.refreshFavorites() }
    }

    Component.onCompleted: refreshFavorites()

    Item {
        anchors.fill: parent
        visible: root.favoritesEmpty

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
                text: qsTr("No favorites")
                typescale: MD.Token.typescale.title_large
            }

            MD.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: Messages.favoritesEmptyHint
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WordWrap
            }
        }
    }

    Flickable {
        anchors.fill: parent
        visible: !root.favoritesEmpty
        contentWidth: width
        contentHeight: contentCol.implicitHeight + pageMargin
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: parent.width
            spacing: MD.Token.spacing.medium

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: pageMargin
                Layout.rightMargin: pageMargin
                Layout.topMargin: pageMargin

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Favorites")
                    typescale: MD.Token.typescale.title_large
                }

                MD.Label {
                    text: qsTr("%1 games").arg(favoritesModel.count)
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_large
                }
            }

            Item {
                id: gridHost
                Layout.fillWidth: true
                Layout.leftMargin: pageMargin
                Layout.rightMargin: pageMargin

                readonly property int gap: root.gridSpacing
                readonly property int columns: {
                    if (width <= 0)
                        return 2
                    return Math.max(2, Math.floor((width + gap) / (root.minCardWidth + gap)))
                }
                readonly property int cellW: columns > 0 ? Math.floor(width / columns) : root.minCardWidth + gap
                readonly property int cardWidth: Math.max(1, cellW - gap)
                readonly property int cardHeight: Math.ceil(cardWidth * 4 / 3) + root.metaHeight
                readonly property int cellH: cardHeight + gap
                readonly property int rows: Math.max(
                    1, Math.ceil(favoritesModel.count / Math.max(1, columns)))
                Layout.preferredHeight: rows * cellH

                GridView {
                    width: gridHost.cellW * gridHost.columns
                    height: parent.height
                    clip: false
                    interactive: false
                    model: favoritesModel
                    cellWidth: gridHost.cellW
                    cellHeight: gridHost.cellH
                    cacheBuffer: 0

                    delegate: Item {
                        id: card
                        width: gridHost.cardWidth
                        height: gridHost.cardHeight

                        required property string gameId
                        required property string title
                        required property string coverUrl
                        required property string sourceName

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.rightMargin: gridHost.gap
                            anchors.bottomMargin: gridHost.gap
                            spacing: MD.Token.spacing.extra_small

                            GamePoster {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                source: card.coverUrl
                                seed: card.title
                                fallbackText: card.title.length > 0 ? card.title.charAt(0) : "?"
                                cornerRadius: MD.Token.shape.corner.extra_large
                                hoverScaleEnabled: true
                                onClicked: root.openGame(card.gameId)
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: card.title
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                typescale: MD.Token.typescale.label_large
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                visible: card.sourceName.length > 0
                                text: card.sourceName
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.label_small
                            }
                        }
                    }
                }
            }
        }
    }
}
