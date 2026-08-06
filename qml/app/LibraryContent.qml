import QtQuick
import QtQuick.Layouts
import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    required property var page

    // ── Populated library ────────────────────────────────────────────────────
    Flickable {
        id: flick
        anchors.fill: parent
        visible: !page.libraryEmpty
        contentWidth: width
        contentHeight: contentCol.implicitHeight + page.pageMargin
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: flick.width
            spacing: MD.Token.spacing.medium

            Item {
                id: heroHost
                Layout.fillWidth: true
                Layout.leftMargin: page.pageMargin
                Layout.rightMargin: page.pageMargin
                Layout.topMargin: page.pageMargin
                Layout.preferredHeight: 248

                Item {
                    id: heroCard
                    anchors.fill: parent
                    clip: true

                    layer.enabled: true
                    layer.effect: MD.RoundClip {
                        corners: MD.Util.corners(page.cardRadius)
                        size: Qt.vector2d(heroCard.width, heroCard.height)
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: MD.Token.color.surface_container
                    }

                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: MD.Util.transparent(MD.Token.color.primary, 0.10)
                            }
                            GradientStop {
                                position: 1.0
                                color: "transparent"
                            }
                        }
                    }

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
                                text: page.heroEyebrow
                                color: MD.Token.color.primary
                                typescale: MD.Token.typescale.label_large
                                elide: Text.ElideRight
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: page.heroTitle
                                typescale: MD.Token.typescale.headline_large
                                elide: Text.ElideRight
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: page.heroSubtitle
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.body_large
                                elide: Text.ElideRight
                                wrapMode: page.heroHasRecentPlay || page.showRunningHero
                                           ? Text.NoWrap
                                           : Text.WordWrap
                                visible: !page.featuredShowJobStatus && !page.showRunningHero
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.extra_small
                                visible: page.showRunningHero

                                MD.Icon {
                                    name: MD.Token.icon.sports_esports
                                    size: 18
                                    color: MD.Token.color.primary
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Running")
                                    color: MD.Token.color.primary
                                    typescale: MD.Token.typescale.body_large
                                    elide: Text.ElideRight
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.extra_small
                                visible: page.featuredShowJobStatus

                                MD.Icon {
                                    name: page.featuredJob.status === "installing"
                                          ? MD.Token.icon.install_desktop
                                          : page.featuredJob.status === "paused"
                                            ? MD.Token.icon.play_arrow
                                            : MD.Token.icon.pause
                                    size: 18
                                    color: MD.Token.color.on_surface_variant
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: page.featuredStatusLine
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_large
                                    elide: Text.ElideRight
                                }
                            }

                            Item { Layout.fillHeight: true }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.small

                                MD.Button {
                                    visible: page.heroHasRecentPlay && !page.showRunningHero
                                    text: qsTr("Play")
                                    mdState.type: MD.Enum.BtFilled
                                    enabled: !!(page.heroGameId)
                                             && Core.isEntryPlayable(page.heroGameId)
                                             && !(Core.runtimeSetupInProgress
                                                  && Core.runtimeSetupGameId === page.heroGameId)
                                    onClicked: Core.launchGame(page.heroGameId)
                                }

                                MD.Button {
                                    visible: page.showRunningHero
                                    text: qsTr("Stop")
                                    mdState.type: MD.Enum.BtFilled
                                    mdState.backgroundColor: MD.Token.color.error
                                    mdState.textColor: MD.Token.color.on_error
                                    onClicked: Core.stopRunningGame()
                                }

                                MD.Button {
                                    visible: page.heroHasRecentPlay || page.showRunningHero
                                    text: qsTr("Details")
                                    mdState.type: MD.Enum.BtOutlined
                                    enabled: !!(page.heroGameId)
                                    onClicked: page.openGame(page.heroGameId)
                                }

                                MD.Button {
                                    visible: page.heroHasUpdate && !page.showRunningHero
                                    text: qsTr("Update")
                                    icon.name: MD.Token.icon.update
                                    mdState.type: MD.Enum.BtFilledTonal
                                    enabled: !!(page.heroGameId)
                                    onClicked: {
                                        if (Core.catalogUpdateHasDlcRisk(page.heroGameId))
                                            dlcUpdateRiskDialog.openForGame(page.heroGameId)
                                        else
                                            Core.updateCatalogEntry(page.heroGameId)
                                    }
                                }
                            }
                        }

                        GamePoster {
                            Layout.preferredWidth: 140
                            Layout.preferredHeight: 186
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                            visible: page.heroHasRecentPlay || page.showRunningHero
                            source: page.heroCoverUrl
                            seed: page.heroTitle
                            fallbackText: (page.heroTitle || "?").charAt(0)
                            cornerRadius: page.cardRadius
                            fillProgress: page.featuredShowJobStatus && !page.showRunningHero
                                          ? page.featuredFillProgress
                                          : -1
                            onClicked: {
                                if (page.heroGameId)
                                    page.openGame(page.heroGameId)
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: page.pageMargin
                Layout.rightMargin: page.pageMargin
                spacing: MD.Token.spacing.medium

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("In library")
                    value: String(Core.library.count)
                    iconName: MD.Token.icon.sports_esports
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Sources")
                    value: String(Core.sources.enabledCount)
                    iconName: MD.Token.icon.storefront
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Tasks")
                    value: String(Core.jobs.count)
                    iconName: MD.Token.icon.downloading
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Updates")
                    value: String(Core.library.updateCount())
                    iconName: MD.Token.icon.update
                }
            }

            MD.Pane {
                Layout.fillWidth: true
                Layout.leftMargin: page.pageMargin
                Layout.rightMargin: page.pageMargin
                visible: Core.jobs.activeCount > 0
                padding: MD.Token.spacing.medium
                radius: page.cardRadius
                backgroundColor: MD.Token.color.secondary_container
                clip: true

                RowLayout {
                    anchors.fill: parent
                    spacing: MD.Token.spacing.medium

                    MD.Icon {
                        name: MD.Token.icon.downloading
                        size: 24
                        color: MD.Token.color.on_secondary_container
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("%1 active downloads").arg(Core.jobs.activeCount)
                            color: MD.Token.color.on_secondary_container
                            typescale: MD.Token.typescale.title_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Downloads continue after restart")
                            color: MD.Token.color.on_secondary_container
                            typescale: MD.Token.typescale.label_medium
                        }
                    }

                    MD.Button {
                        text: qsTr("Open")
                        onClicked: page.openDownloads()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: page.pageMargin
                Layout.rightMargin: page.pageMargin
                spacing: MD.Token.spacing.small

                RowLayout {
                    Layout.fillWidth: true

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("My library")
                        typescale: MD.Token.typescale.title_large
                    }

                    MD.Label {
                        text: qsTr("%1 games").arg(Core.library.count)
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_large
                    }
                }

                Item {
                    id: gridHost
                    Layout.fillWidth: true

                    // N cards + (N-1) gaps: floor((w + gap) / (min + gap)).
                    // Integer cellW so columns * cellW never exceeds width
                    // (float width/columns clipped the last poster under clip:true).
                    readonly property int gap: page.gridSpacing
                    readonly property int columns: {
                        if (width <= 0)
                            return 2
                        return Math.max(
                            2,
                            Math.floor((width + gap) / (page.minCardWidth + gap)))
                    }
                    readonly property int cellW: columns > 0
                        ? Math.floor(width / columns)
                        : page.minCardWidth + gap
                    readonly property int cardWidth: Math.max(1, cellW - gap)
                    readonly property int cardHeight: Math.ceil(cardWidth * 4 / 3) + page.metaHeight
                    readonly property int cellH: cardHeight + gap
                    readonly property int rows: Math.max(
                        1, Math.ceil(Core.library.count / Math.max(1, columns)))
                    // Full cell rows so GridView never clips the next line.
                    Layout.preferredHeight: rows * cellH

                    GridView {
                        // Exact tile width - leftover pixels stay empty on the right.
                        width: gridHost.cellW * gridHost.columns
                        height: parent.height
                        clip: false
                        interactive: false
                        model: Core.library
                        cellWidth: gridHost.cellW
                        cellHeight: gridHost.cellH
                        cacheBuffer: 0
                        delegate: LibraryGameCard {
                            width: gridHost.cardWidth
                            height: gridHost.cardHeight
                            onOpenDetails: function (id) { page.openGame(id) }
                        }
                    }
                }
            }
        }
    }

    DlcUpdateRiskDialog {
        id: dlcUpdateRiskDialog
        width: Math.min(420, page.width > 0 ? page.width - 48 : 420)
        onUpdateAccepted: function(gameId) {
            Core.updateCatalogEntry(gameId)
        }
    }
}
