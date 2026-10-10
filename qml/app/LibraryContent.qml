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
        opacity: !page.libraryEmpty ? 1 : 0
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation {
                duration: AppMotion.short
                easing: AppMotion.standard
            }
        }
        contentWidth: width
        contentHeight: contentCol.implicitHeight + page.pageMargin
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: flick.width
            spacing: MD.Token.spacing.medium

            // v2 page header: a big title and the library at a glance as small chips.
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: page.pageMargin
                Layout.rightMargin: page.pageMargin
                Layout.topMargin: page.pageMargin
                spacing: MD.Token.spacing.medium

                DisplayTitle {
                    text: qsTr("Library")
                }

                Item { Layout.fillWidth: true }

                Repeater {
                    model: [
                        { icon: MD.Token.icon.sports_esports, text: qsTr("%n game(s)", "", Core.library.count), act: "" },
                        { icon: MD.Token.icon.storefront, text: qsTr("%n source(s)", "", Core.sources.enabledCount), act: "sources" },
                        { icon: MD.Token.icon.downloading, text: qsTr("%n task(s)", "", Core.jobs.count), act: "downloads" },
                        { icon: MD.Token.icon.update, text: qsTr("%n update(s)", "", (void page.libraryRevision, Core.library.updateCount())), act: "" }
                    ]

                    delegate: DetailsChip {
                        required property var modelData
                        text: modelData.text
                        icon.name: modelData.icon
                        onClicked: {
                            if (modelData.act === "downloads")
                                page.openDownloads()
                            else if (modelData.act === "sources")
                                page.addSourceRequested()
                        }
                    }
                }
            }

            Item {
                id: heroHost
                Layout.fillWidth: true
                Layout.leftMargin: page.pageMargin
                Layout.rightMargin: page.pageMargin
                Layout.topMargin: MD.Token.spacing.small
                // Nothing played yet: a compact invitation instead of a big empty card.
                readonly property bool hasHero: page.heroHasRecentPlay || page.showRunningHero
                                               || page.featuredShowJobStatus
                Layout.preferredHeight: hasHero ? 248 : 200
                Behavior on Layout.preferredHeight {
                    NumberAnimation { duration: AppMotion.medium; easing: AppMotion.emphasizedDecelerate }
                }

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

                        ShapeIcon {
                            visible: !heroHost.hasHero
                            Layout.preferredWidth: 96
                            Layout.preferredHeight: 96
                            Layout.alignment: Qt.AlignVCenter
                            iconName: MD.Token.icon.sports_esports
                            kind: "burst"
                            iconSize: 40
                        }

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
                                font.weight: Font.DemiBold
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
                                    visible: !heroHost.hasHero
                                    text: qsTr("Open catalog")
                                    icon.name: MD.Token.icon.storefront
                                    mdState.type: MD.Enum.BtFilledTonal
                                    onClicked: page.openCatalog()
                                }

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
                        text: qsTr("%n game(s)", "", Core.library.count)
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
                        populate: StaggerEnterTransition {}
                        add: StaggerEnterTransition { staggered: false }
                        delegate: LibraryGameCard {
                            width: gridHost.cardWidth
                            height: gridHost.cardHeight
                            onOpenDetails: function (id) { page.openGame(id) }
                            onRequestUpdate: function (id) {
                                if (Core.catalogUpdateHasDlcRisk(id))
                                    dlcUpdateRiskDialog.openForGame(id)
                                else
                                    Core.updateCatalogEntry(id)
                            }
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
