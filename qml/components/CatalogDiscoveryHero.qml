import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property var page
    required property var shelfModel

    signal openGame(string entryId)

    property int featuredIndex: 0
    property string heroLocalUrl: ""

    readonly property int shelfCount: shelfModel ? shelfModel.count : 0
    readonly property var featured: shelfCount > 0
                                    ? shelfModel.entryInfo(Math.min(featuredIndex, shelfCount - 1))
                                    : null

    readonly property string featuredEntryId: featured ? (featured.entryId || "") : ""

    function refreshHeroCover() {
        heroLocalUrl = ""
        if (!featuredEntryId.length)
            return
        const cached = Core.catalogHeroCoverUrl(featuredEntryId)
        if (cached && cached.length) {
            heroLocalUrl = cached
            return
        }
        // Prefer card cover while hero banner downloads.
        if (featured && (featured.coverUrl || "").startsWith("file:"))
            heroLocalUrl = featured.coverUrl
        Core.requestCatalogHeroCover(featuredEntryId)
        Core.requestCatalogCover(featuredEntryId)
    }

    onFeaturedEntryIdChanged: refreshHeroCover()
    onShelfCountChanged: {
        if (featuredIndex >= shelfCount)
            featuredIndex = 0
        refreshHeroCover()
    }
    Component.onCompleted: refreshHeroCover()

    Connections {
        target: Core
        function onCatalogHeroCoverChanged(entryId) {
            if (entryId !== root.featuredEntryId)
                return
            root.heroLocalUrl = Core.catalogHeroCoverUrl(entryId)
        }
        function onEntryMetadataChanged(entryId) {
            if (entryId !== root.featuredEntryId)
                return
            if (!(root.heroLocalUrl || "").startsWith("file:")
                    && featured && (featured.coverUrl || "").startsWith("file:"))
                root.heroLocalUrl = featured.coverUrl
        }
    }

    function nextPick() {
        if (shelfCount <= 1)
            return
        featuredIndex = (featuredIndex + 1) % shelfCount
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

        // Round via poster + parent radius; avoid layer RoundClip (it clips title text).
        GamePoster {
            id: heroPoster
            anchors.fill: parent
            source: root.heroLocalUrl
            seed: featured ? featured.title : ""
            fallbackText: featured ? featured.title : "?"
            awaiting: featured ? featured.metadataPending : false
            allowRemote: false
            cornerRadius: page.cardRadius
            decodeWidth: 1440
            decodeHeight: 540
            enableShimmer: true
            onLoadFailed: {
                if ((root.heroLocalUrl || "").startsWith("file:"))
                    Core.invalidateCatalogCover(root.featuredEntryId)
            }
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
            }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            onClicked: if (featured) root.openGame(featured.entryId)
        }
    }
}
