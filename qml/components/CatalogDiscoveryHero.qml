import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property var page
    required property var shelfModel

    signal openGame(string entryId)
    signal surpriseRequested()

    property int featuredIndex: 0

    readonly property int shelfCount: shelfModel ? shelfModel.count : 0
    readonly property var featured: shelfCount > 0
                                    ? shelfModel.entryInfo(Math.min(featuredIndex, shelfCount - 1))
                                    : null
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
    onShelfCountChanged: {
        if (featuredIndex >= shelfCount)
            featuredIndex = 0
    }

    function nextPick() {
        if (shelfCount <= 1)
            return
        featuredIndex = (featuredIndex + 1) % shelfCount
    }

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

    readonly property string metaLine: {
        if (!featured)
            return ""
        const parts = []
        if (root.playersLabel.length)
            parts.push(root.playersLabel)
        if ((featured.sizeLabel || "").length)
            parts.push(featured.sizeLabel)
        return parts.join(" · ")
    }

    implicitHeight: visible ? 300 : 0
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
            decodeWidth: 1440
            decodeHeight: 540
            enableShimmer: true
            onLoadFailed: root.handleHeroLoadFailed()
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: MD.Util.transparent(MD.Token.color.surface, 0.96) }
                GradientStop { position: 0.48; color: MD.Util.transparent(MD.Token.color.surface, 0.55) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.45; color: "transparent" }
                GradientStop { position: 1.0; color: MD.Util.transparent(MD.Token.color.surface, 0.45) }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: MD.Token.spacing.large
            spacing: MD.Token.spacing.small

            MD.Label {
                text: qsTr("Tonight's pick")
                color: MD.Token.color.primary
                typescale: MD.Token.typescale.label_large
            }

            MD.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: parent.width * 0.62
                text: featured ? featured.title : ""
                typescale: MD.Token.typescale.headline_large
                elide: Text.ElideRight
                wrapMode: Text.WordWrap
                maximumLineCount: 2
            }

            MD.Label {
                Layout.fillWidth: true
                Layout.maximumWidth: parent.width * 0.62
                visible: root.metaLine.length > 0
                text: root.metaLine
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_large
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                spacing: MD.Token.spacing.small

                MD.Button {
                    mdState.type: MD.Enum.BtFilled
                    text: qsTr("Open game")
                    onClicked: if (featured) root.openGame(featured.entryId)
                }

                MD.Button {
                    mdState.type: MD.Enum.BtFilledTonal
                    text: qsTr("Another pick")
                    visible: root.shelfCount > 1
                    onClicked: root.nextPick()
                }

                MD.Button {
                    mdState.type: MD.Enum.BtOutlined
                    text: qsTr("Surprise me")
                    onClicked: root.surpriseRequested()
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            onClicked: if (featured) root.openGame(featured.entryId)
        }
    }
}
