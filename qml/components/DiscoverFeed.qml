import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtMultimedia

import Arachnel.Core 1.0
import Qcm.Material as MD

// Discover as a feed: one game per screen, its trailer playing (muted) or its screenshots
// drifting by, the essentials over it and actions on the right like a short-video app. The wheel,
// the arrow keys or the buttons move to the next game; the feed never runs dry - after the curated
// shelves it keeps going with random games from the catalog.
FocusScope {
    id: root

    required property var page

    signal openGame(string entryId)

    // ---------- Feeds ----------
    readonly property var feeds: [
        { key: "foryou", label: qsTr("For you") },
        { key: "friends", label: qsTr("With friends") },
        { key: "new", label: qsTr("New") },
        { key: "solo", label: qsTr("Solo") }
    ]
    property int feedIndex: 0
    property bool muted: true
    property int revision: 0
    property var hidden: ({})

    function shelfIds(shelf) {
        const out = []
        if (!shelf)
            return out
        for (let i = 0; i < shelf.count; ++i) {
            const info = shelf.entryInfo(i)
            if (info && info.entryId)
                out.push(info.entryId)
        }
        return out
    }

    function idsFor(key) {
        const d = Core.catalogDiscovery
        if (key === "friends")
            return shelfIds(d.friendsShelf).concat(shelfIds(d.onlineFixShelf))
        if (key === "new")
            return shelfIds(d.newShelf)
        if (key === "solo")
            return shelfIds(d.soloShelf)
        // For you: interleave every shelf so the feed stays varied.
        const lists = [shelfIds(d.onFireShelf), shelfIds(d.friendsShelf), shelfIds(d.newShelf),
                       shelfIds(d.soloShelf), shelfIds(d.onlineFixShelf)]
        const out = []
        for (let i = 0; lists.some(l => i < l.length); ++i) {
            for (let k = 0; k < lists.length; ++k) {
                if (i < lists[k].length)
                    out.push(lists[k][i])
            }
        }
        return out
    }

    function rebuild() {
        const seen = {}
        feedModel.clear()
        for (const id of idsFor(feeds[feedIndex].key)) {
            if (seen[id] || hidden[id])
                continue
            seen[id] = true
            feedModel.append({ entryId: id })
        }
        topUp(20)
        view.currentIndex = 0
        view.positionViewAtBeginning()
    }

    // Endless: random catalog games with a Steam page (so they have media).
    function topUp(count) {
        const have = []
        for (let i = 0; i < feedModel.count; ++i)
            have.push(feedModel.get(i).entryId)
        for (const id of Core.randomCatalogEntryIds(count, have)) {
            if (!hidden[id])
                feedModel.append({ entryId: id })
        }
    }

    function go(delta) {
        const next = Math.max(0, Math.min(feedModel.count - 1, view.currentIndex + delta))
        view.currentIndex = next
    }

    function hideCurrent() {
        const i = view.currentIndex
        if (i < 0 || i >= feedModel.count)
            return
        const next = Object.assign({}, hidden)
        next[feedModel.get(i).entryId] = true
        hidden = next
        feedModel.remove(i)
        if (feedModel.count - view.currentIndex < 5)
            topUp(10)
    }

    // Fetch what the next cards need before they are reached.
    function warm(index) {
        for (let k = index; k < Math.min(feedModel.count, index + 3); ++k) {
            const id = feedModel.get(k).entryId
            Core.enrichCatalogEntry(id)
            Core.requestCatalogHeroCover(id)
            Core.requestCatalogCover(id)
            const trailer = Core.entryDetails(id).trailerUrl || ""
            if (trailer.length)
                Core.prefetchTrailer(Core.trailerPreviewUrl(trailer))
        }
    }

    ListModel { id: feedModel }

    Component.onCompleted: Qt.callLater(rebuild)
    onFeedIndexChanged: rebuild()

    Connections {
        target: Core.catalogDiscovery
        function onShelvesChanged() {
            // Fill an empty feed when the shelves arrive; never reshuffle under the reader.
            if (view.currentIndex <= 0)
                root.rebuild()
        }
    }

    Connections {
        target: Core
        function onEntryMetadataChanged(entryId) { root.revision++ }
        function onCatalogHeroCoverChanged(entryId) { root.revision++ }
        function onCatalogCoverChanged(entryId) { root.revision++ }
    }

    Keys.onPressed: function (event) {
        switch (event.key) {
        case Qt.Key_Down:
        case Qt.Key_J:
        case Qt.Key_Space:
        case Qt.Key_PageDown:
            root.go(1); event.accepted = true; break
        case Qt.Key_Up:
        case Qt.Key_K:
        case Qt.Key_PageUp:
            root.go(-1); event.accepted = true; break
        case Qt.Key_M:
            root.muted = !root.muted; event.accepted = true; break
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (view.currentItem)
                root.openGame(view.currentItem.entryId)
            event.accepted = true
            break
        }
    }

    // ================= Layout =================
    ColumnLayout {
        anchors.fill: parent
        spacing: MD.Token.spacing.medium

        // Title and feed switcher. On a narrow window the switcher moves to its own row and
        // "All games" shrinks to an icon, so nothing is pushed off screen.
        readonly property bool wideHeader: root.width >= 1050

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.large

            DisplayTitle { text: qsTr("Discover") }

            ConnectedTextToggle {
                visible: parent.parent.wideHeader
                labels: root.feeds.map(f => f.label)
                currentIndex: root.feedIndex
                onPicked: function (index) {
                    root.feedIndex = index
                    root.forceActiveFocus()
                }
            }

            Item { Layout.fillWidth: true }

            MD.Button {
                visible: parent.parent.wideHeader
                mdState.type: MD.Enum.BtOutlined
                text: qsTr("All games")
                icon.name: MD.Token.icon.arrow_forward
                onClicked: root.page.openFullCatalog("")
            }
            MD.IconButton {
                visible: !parent.parent.wideHeader
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.storefront
                Accessible.name: qsTr("All games")
                onClicked: root.page.openFullCatalog("")
            }

            MD.IconButton {
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.refresh
                Accessible.name: qsTr("Refresh")
                onClicked: {
                    Core.catalogDiscovery.refresh()
                    root.rebuild()
                }
            }
        }

        ConnectedTextToggle {
            visible: !parent.wideHeader
            labels: root.feeds.map(f => f.label)
            currentIndex: root.feedIndex
            onPicked: function (index) {
                root.feedIndex = index
                root.forceActiveFocus()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: MD.Token.spacing.large

            // ---------- The feed ----------
            ListView {
                id: view
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: feedModel
                orientation: ListView.Vertical
                interactive: false
                snapMode: ListView.SnapOneItem
                highlightRangeMode: ListView.StrictlyEnforceRange
                preferredHighlightBegin: 0
                preferredHighlightEnd: height
                highlightMoveDuration: AppMotion.scaled(520)
                highlightMoveVelocity: -1
                cacheBuffer: height
                spacing: MD.Token.spacing.medium

                onCurrentIndexChanged: {
                    root.warm(currentIndex)
                    if (feedModel.count - currentIndex < 6)
                        root.topUp(15)
                }

                Timer {
                    id: wheelCooldown
                    interval: 420
                }

                WheelHandler {
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: function (event) {
                        if (wheelCooldown.running || Math.abs(event.angleDelta.y) < 30)
                            return
                        root.go(event.angleDelta.y < 0 ? 1 : -1)
                        wheelCooldown.start()
                    }
                }

                delegate: Item {
                    id: card
                    required property int index
                    required property string entryId

                    readonly property bool current: ListView.isCurrentItem
                    readonly property var info: {
                        void root.revision
                        return Core.entryDetails(card.entryId)
                    }
                    readonly property var shots: info.screenshotUrls ?? []
                    readonly property string trailer: info.trailerUrl ?? ""
                    readonly property string banner: {
                        void root.revision
                        const hero = Core.catalogHeroCoverUrl(card.entryId)
                        return hero && hero.length ? hero : (shots.length ? shots[0] : (info.coverUrl ?? ""))
                    }
                    readonly property var genres: String(info.genres ?? "").split(",")
                        .map(t => t.trim()).filter(t => t.length && t.toLowerCase() !== "drm").slice(0, 3)
                    readonly property bool favorited: Core.settings.bookmarkedEntryIds.indexOf(card.entryId) >= 0

                    width: view.width
                    height: view.height

                    // ---------- Media ----------
                    Item {
                        id: media
                        anchors.fill: parent
                        clip: true

                        Rectangle {
                            anchors.fill: parent
                            radius: 32
                            color: MD.Token.color.surface_container_high
                        }

                        // Still: hero banner (or first screenshot) while not playing.
                        MD.Image {
                            anchors.fill: parent
                            radius: 32
                            source: card.banner
                            sourceSize: Qt.size(1280, 720)
                            asynchronous: true
                        }

                        // Screenshots drift and crossfade when the game has no trailer.
                        Item {
                            anchors.fill: parent
                            visible: card.current && !card.trailer.length && card.shots.length > 1
                            layer.enabled: visible
                            layer.effect: MD.RoundClip {
                                corners: MD.Util.corners(32)
                                size: Qt.vector2d(media.width, media.height)
                            }

                            property int shot: 0
                            Timer {
                                interval: 3600
                                repeat: true
                                running: parent.visible && root.visible
                                onTriggered: parent.shot = (parent.shot + 1) % Math.min(card.shots.length, 6)
                            }

                            Repeater {
                                model: Math.min(card.shots.length, 6)
                                Image {
                                    required property int index
                                    anchors.fill: parent
                                    source: Math.abs(index - parent.shot) <= 1 || index === 0 ? card.shots[index] : ""
                                    sourceSize: Qt.size(1280, 720)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    opacity: index === parent.shot ? 1 : 0
                                    Behavior on opacity { NumberAnimation { duration: AppMotion.scaled(900) } }
                                }
                            }
                        }

                        // The trailer, muted, looping - only for the card in view.
                        Loader {
                            anchors.fill: parent
                            active: card.current && card.trailer.length > 0 && root.visible && !AppMotion.reduceMotion
                            sourceComponent: Component {
                                Item {
                                    layer.enabled: true
                                    layer.effect: MD.RoundClip {
                                        corners: MD.Util.corners(32)
                                        size: Qt.vector2d(media.width, media.height)
                                    }

                                    MediaPlayer {
                                        id: player
                                        source: Core.trailerSource(Core.trailerPreviewUrl(card.trailer))
                                        loops: MediaPlayer.Infinite
                                        videoOutput: videoOut
                                        audioOutput: AudioOutput { muted: root.muted; volume: 0.6 }
                                        Component.onCompleted: play()
                                        // No short clip next to this trailer: play the full one.
                                        onErrorOccurred: {
                                            if (String(source) !== card.trailer) {
                                                source = card.trailer
                                                play()
                                            }
                                        }
                                    }
                                    VideoOutput {
                                        id: videoOut
                                        anchors.fill: parent
                                        fillMode: VideoOutput.PreserveAspectCrop
                                        opacity: player.playbackState === MediaPlayer.PlayingState ? 1 : 0
                                        Behavior on opacity { NumberAnimation { duration: AppMotion.long } }
                                    }
                                }
                            }
                        }

                        // Legibility.
                        Rectangle {
                            anchors.fill: parent
                            radius: 32
                            gradient: Gradient {
                                GradientStop { position: 0.3; color: "transparent" }
                                GradientStop { position: 0.7; color: Qt.rgba(0, 0, 0, 0.55) }
                                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.92) }
                            }
                        }
                    }

                    // ---------- Info ----------
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: actions.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 32
                        anchors.rightMargin: MD.Token.spacing.large
                        spacing: MD.Token.spacing.small

                        opacity: card.current ? 1 : 0.4
                        transform: Translate { y: card.current ? 0 : 24 }
                        Behavior on opacity { NumberAnimation { duration: AppMotion.long; easing: AppMotion.emphasizedDecelerate } }

                        Flow {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small
                            visible: card.genres.length > 0 || (card.info.sizeLabel ?? "").length > 0

                            Repeater {
                                model: card.genres
                                Rectangle {
                                    required property string modelData
                                    implicitWidth: genreLabel.implicitWidth + 20
                                    implicitHeight: 28
                                    radius: 14
                                    color: Qt.rgba(1, 1, 1, 0.16)
                                    MD.Label {
                                        id: genreLabel
                                        anchors.centerIn: parent
                                        text: parent.modelData
                                        color: "white"
                                        typescale: MD.Token.typescale.label_large
                                    }
                                }
                            }
                            Rectangle {
                                visible: (card.info.sizeLabel ?? "").length > 0
                                implicitWidth: sizeLabel.implicitWidth + 20
                                implicitHeight: 28
                                radius: 14
                                color: Qt.rgba(1, 1, 1, 0.16)
                                MD.Label {
                                    id: sizeLabel
                                    anchors.centerIn: parent
                                    text: card.info.sizeLabel ?? ""
                                    color: "white"
                                    typescale: MD.Token.typescale.label_large
                                }
                            }
                        }

                        DisplayTitle {
                            Layout.fillWidth: true
                            color: "white"
                            size: (card.info.title ?? "").length > 26 ? 34 : 44
                            text: card.info.title ?? ""
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            Layout.maximumWidth: 720
                            visible: text.length > 0
                            text: card.info.description ?? ""
                            color: Qt.rgba(1, 1, 1, 0.82)
                            typescale: MD.Token.typescale.body_large
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        MD.Button {
                            Layout.topMargin: MD.Token.spacing.small
                            mdState.type: MD.Enum.BtFilled
                            mdState.size: MD.Enum.M
                            text: qsTr("Open game")
                            icon.name: MD.Token.icon.arrow_forward
                            onClicked: root.openGame(card.entryId)
                        }
                    }

                    // ---------- Actions (short-video style) ----------
                    Column {
                        id: actions
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 24
                        spacing: MD.Token.spacing.medium

                        FeedAction {
                            iconName: MD.Token.icon.favorite
                            filled: card.favorited
                            accent: card.favorited
                            label: card.favorited ? qsTr("Saved") : qsTr("Save")
                            onTriggered: Core.toggleBookmark(card.entryId)
                        }
                        FeedAction {
                            iconName: MD.Token.icon.open_in_full
                            label: qsTr("Details")
                            onTriggered: root.openGame(card.entryId)
                        }
                        FeedAction {
                            visible: card.trailer.length > 0
                            iconName: root.muted ? MD.Token.icon.volume_off : MD.Token.icon.volume_up
                            label: root.muted ? qsTr("Sound") : qsTr("Mute")
                            onTriggered: root.muted = !root.muted
                        }
                        FeedAction {
                            iconName: MD.Token.icon.thumb_down
                            label: qsTr("Not for me")
                            onTriggered: root.hideCurrent()
                        }
                    }

                    TapHandler {
                        onTapped: root.forceActiveFocus()
                        onDoubleTapped: Core.toggleBookmark(card.entryId)
                    }
                }
            }

            // ---------- Up next ----------
            ColumnLayout {
                Layout.preferredWidth: 260
                Layout.maximumWidth: 260
                Layout.fillWidth: false
                Layout.fillHeight: true
                visible: root.width > 1100
                spacing: MD.Token.spacing.small

                RowLayout {
                    Layout.fillWidth: true

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Up next")
                        typescale: MD.Token.typescale.title_medium
                    }
                    MD.IconButton {
                        mdState.type: MD.Enum.IBtFilledTonal
                        icon.name: MD.Token.icon.keyboard_arrow_up
                        enabled: view.currentIndex > 0
                        onClicked: root.go(-1)
                    }
                    MD.IconButton {
                        mdState.type: MD.Enum.IBtFilledTonal
                        icon.name: MD.Token.icon.keyboard_arrow_down
                        onClicked: root.go(1)
                    }
                }

                // Fills the column down to the bottom of the card.
                ListView {
                    id: upNext
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: MD.Token.spacing.small
                    interactive: contentHeight > height
                    boundsBehavior: Flickable.StopAtBounds
                    model: Math.max(0, Math.min(20, feedModel.count - view.currentIndex - 1))

                    delegate: Rectangle {
                        id: nextRow
                        required property int index
                        width: upNext.width
                        height: 72
                        readonly property int target: view.currentIndex + 1 + index
                        readonly property string entryId: target < feedModel.count ? feedModel.get(target).entryId : ""
                        readonly property var info: {
                            void root.revision
                            void feedModel.count
                            return entryId.length ? Core.entryDetails(entryId) : ({})
                        }

                        visible: entryId.length > 0
                        onEntryIdChanged: if (entryId.length) Qt.callLater(Core.requestCatalogCover, entryId)
                        radius: 20
                        color: rowHover.hovered ? MD.Token.color.surface_container_high : MD.Token.color.surface_container
                        Behavior on color { ColorAnimation { duration: AppMotion.short } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: MD.Token.spacing.small

                            MD.Image {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 56
                                radius: 10
                                source: String(nextRow.info.coverUrl ?? "")
                                sourceSize: Qt.size(84, 112)
                                asynchronous: true
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                MD.Label {
                                    Layout.fillWidth: true
                                    text: nextRow.info.title ?? ""
                                    typescale: MD.Token.typescale.title_small
                                    elide: Text.ElideRight
                                    maximumLineCount: 2
                                    wrapMode: Text.WordWrap
                                }
                                MD.Label {
                                    Layout.fillWidth: true
                                    text: nextRow.info.sizeLabel ?? ""
                                    typescale: MD.Token.typescale.label_small
                                    color: MD.Token.color.on_surface_variant
                                }
                            }
                        }

                        HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: view.currentIndex = nextRow.target }
                    }
                }

            }
        }
    }
}
