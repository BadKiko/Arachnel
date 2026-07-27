import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property var page
    required property var shelfModel

    signal openGame(string entryId)

    readonly property var featured: shelfModel && shelfModel.count > 0
                                    ? shelfModel.entryInfo(0) : null
    readonly property string heroCoverUrl: {
        if (!featured)
            return ""
        const appId = featured.steamAppId || ""
        if (appId.length > 0)
            return "https://cdn.akamai.steamstatic.com/steam/apps/" + appId + "/library_hero.jpg"
        return featured.coverUrl || ""
    }
    property string heroImageUrl: heroCoverUrl

    onHeroCoverUrlChanged: heroImageUrl = heroCoverUrl

    function handleHeroLoadFailed() {
        if (!featured)
            return
        if (heroImageUrl.indexOf("library_hero") >= 0 && (featured.steamAppId || "").length > 0) {
            heroImageUrl = "https://cdn.akamai.steamstatic.com/steam/apps/"
                           + featured.steamAppId + "/header.jpg"
            return
        }
        Core.invalidateCatalogCover(featured.entryId)
    }
    readonly property string playersLabel: {
        if (!featured || featured.currentPlayers < 0)
            return ""
        const n = featured.currentPlayers
        if (n >= 1000000)
            return (n / 1000000).toFixed(1) + "M " + qsTr("playing now")
        if (n >= 1000)
            return (n / 1000).toFixed(1) + "K " + qsTr("playing now")
        return n + " " + qsTr("playing now")
    }

    implicitHeight: visible ? 220 : 0
    visible: featured !== null

    Rectangle {
        id: heroCard
        anchors.fill: parent
        radius: page.cardRadius
        color: MD.Token.color.surface_container_high
        clip: true

        GamePoster {
            id: heroPoster
            anchors.fill: parent
            source: root.heroImageUrl
            seed: featured ? featured.title : ""
            fallbackText: featured ? featured.title : "?"
            awaiting: featured ? featured.metadataPending : false
            cornerRadius: 0
            decodeWidth: 1280
            decodeHeight: 480
            enableShimmer: true
            onLoadFailed: root.handleHeroLoadFailed()
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: MD.Util.transparent(MD.Token.color.surface, 0.94) }
                GradientStop { position: 0.52; color: MD.Util.transparent(MD.Token.color.surface, 0.5) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.55; color: "transparent" }
                GradientStop { position: 1.0; color: MD.Util.transparent(MD.Token.color.surface, 0.35) }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: MD.Token.spacing.large
            spacing: MD.Token.spacing.large

            ColumnLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: parent.width * 0.58
                spacing: MD.Token.spacing.small

                MD.Label {
                    text: qsTr("Trending pick")
                    color: MD.Token.color.primary
                    typescale: MD.Token.typescale.label_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: featured ? featured.title : ""
                    typescale: MD.Token.typescale.headline_large
                    elide: Text.ElideRight
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                }

                MD.Label {
                    Layout.fillWidth: true
                    visible: root.playersLabel.length > 0
                    text: root.playersLabel
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    visible: featured && (featured.sizeLabel || "").length > 0
                    text: featured ? featured.sizeLabel : ""
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }

                RowLayout {
                    spacing: MD.Token.spacing.small
                    Layout.topMargin: MD.Token.spacing.extra_small

                    MD.Button {
                        mdState.type: MD.Enum.BtFilled
                        text: qsTr("View game")
                        onClicked: if (featured) root.openGame(featured.entryId)
                    }

                    MD.Button {
                        mdState.type: MD.Enum.BtTonal
                        text: qsTr("Browse all")
                        onClicked: page.browseAllMode = true
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            onClicked: if (featured) root.openGame(featured.entryId)
        }
    }
}
