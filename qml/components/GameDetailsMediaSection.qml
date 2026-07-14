import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtMultimedia

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var screenshotUrls
    required property string trailerUrl
    required property string trailerThumbnailUrl
    property bool loading: false

    readonly property bool hasScreenshots: root.screenshotUrls && root.screenshotUrls.length > 0
    readonly property bool hasTrailer: (root.trailerUrl ?? "").length > 0
    readonly property bool hasMedia: root.hasScreenshots || root.hasTrailer
    readonly property bool showSection: root.hasMedia || root.loading
    readonly property bool showSkeleton: root.loading && !root.hasMedia
    readonly property real mediaCornerRadius: MD.Token.shape.corner.large
    readonly property real stripTileWidth: 234
    readonly property real stripTileHeight: 132
    readonly property int skeletonTileCount: 5

    readonly property var mediaStripModel: {
        const items = []
        if (root.hasTrailer) {
            items.push({
                kind: "trailer",
                thumb: root.trailerThumbnailUrl ?? ""
            })
        }
        const shots = root.screenshotUrls ?? []
        for (let i = 0; i < shots.length; ++i) {
            items.push({
                kind: "screenshot",
                url: shots[i],
                shotIndex: i
            })
        }
        return items
    }

    readonly property var stripModel: root.showSkeleton
                                    ? Array.from({ length: root.skeletonTileCount },
                                                 (_, i) => ({ kind: "skeleton", slot: i }))
                                    : root.mediaStripModel

    function openScreenshotPreview(index) {
        if (!root.hasScreenshots)
            return
        const clamped = Math.max(0, Math.min(index, root.screenshotUrls.length - 1))
        screenshotPreview.currentIndex = clamped
        screenshotPreview.open()
    }

    function openTrailerPreview() {
        if (!root.hasTrailer)
            return
        trailerPreview.open()
    }

    function toggleTrailerPlayback() {
        const player = trailerPlayerLoader.item?.player
        if (!player)
            return
        if (player.playbackState === MediaPlayer.PlayingState)
            player.pause()
        else
            player.play()
    }

    visible: root.showSection
    spacing: MD.Token.spacing.medium

    opacity: root.showSection ? 1 : 0
    Behavior on opacity {
        NumberAnimation {
            duration: MD.Token.duration.medium2
            easing.type: Easing.OutCubic
        }
    }

    MD.Label {
        Layout.fillWidth: true
        text: qsTr("Screenshots")
        typescale: MD.Token.typescale.title_medium
    }

    ListView {
        id: mediaStrip
        Layout.fillWidth: true
        Layout.preferredHeight: root.stripTileHeight
        orientation: ListView.Horizontal
        spacing: MD.Token.spacing.small
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        model: root.stripModel

        delegate: Item {
            id: mediaTile
            required property int index
            required property var modelData

            readonly property bool isSkeleton: (modelData?.kind ?? "") === "skeleton"
            readonly property bool isTrailer: !isSkeleton && (modelData?.kind ?? "") === "trailer"
            readonly property string imageSource: isTrailer
                                                ? (modelData?.thumb ?? "")
                                                : (modelData?.url ?? "")

            width: root.stripTileWidth
            height: root.stripTileHeight

            opacity: mediaTile.isSkeleton ? 1 : (mediaTile.imageReady ? 1 : 0.92)
            scale: mediaTile.isSkeleton ? 1 : (mediaTile.imageReady ? 1 : 0.985)

            Behavior on opacity {
                NumberAnimation {
                    duration: MD.Token.duration.short4
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: MD.Token.duration.short4
                    easing.type: Easing.OutCubic
                }
            }

            Image {
                id: imageProbe
                visible: false
                source: mediaTile.isSkeleton ? "" : mediaTile.imageSource
                asynchronous: true
            }

            readonly property bool imageReady: mediaTile.isSkeleton
                                             || mediaTile.imageSource.length === 0
                                             || imageProbe.status === Image.Ready
            readonly property bool imageLoading: !mediaTile.isSkeleton
                                               && mediaTile.imageSource.length > 0
                                               && !mediaTile.imageReady

            // Skeleton / placeholder while metadata or image loads
            Item {
                anchors.fill: parent
                visible: mediaTile.isSkeleton || mediaTile.imageLoading

                Rectangle {
                    anchors.fill: parent
                    radius: root.mediaCornerRadius
                    color: MD.Token.color.surface_container_high
                    clip: true

                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop {
                                position: 0
                                color: MD.Util.transparent(MD.Token.color.primary, 0.05)
                            }
                            GradientStop {
                                position: 1
                                color: MD.Util.transparent(MD.Token.color.primary, 0.01)
                            }
                        }
                    }

                    Item {
                        id: shimmerClip
                        anchors.fill: parent
                        clip: true

                        Rectangle {
                            id: shimmerBand
                            width: parent.width * 0.42
                            height: parent.height * 1.5
                            rotation: 16
                            anchors.verticalCenter: parent.verticalCenter
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0; color: "transparent" }
                                GradientStop {
                                    position: 0.5
                                    color: MD.Util.transparent(MD.Token.color.on_surface, 0.12)
                                }
                                GradientStop { position: 1; color: "transparent" }
                            }

                            SequentialAnimation on x {
                                running: shimmerClip.visible
                                loops: Animation.Infinite
                                NumberAnimation {
                                    from: -shimmerBand.width
                                    to: mediaTile.width + shimmerBand.width
                                    duration: 1300
                                    easing.type: Easing.InOutCubic
                                }
                                PauseAnimation { duration: 220 }
                            }
                        }
                    }
                }

                MD.Icon {
                    anchors.centerIn: parent
                    visible: mediaTile.isSkeleton ? mediaTile.index === 0 : mediaTile.isTrailer
                    name: MD.Token.icon.play_arrow
                    size: mediaTile.isSkeleton ? 36 : 40
                    color: MD.Token.color.on_surface_variant
                    opacity: 0.45
                }
            }

            MD.Image {
                anchors.fill: parent
                source: mediaTile.imageSource
                radius: root.mediaCornerRadius
                elevation: MD.Token.elevation.level0
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: !mediaTile.isSkeleton && mediaTile.imageSource.length > 0
                opacity: mediaTile.imageReady ? 1 : 0

                Behavior on opacity {
                    NumberAnimation {
                        duration: MD.Token.duration.short4
                        easing.type: Easing.OutCubic
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: root.mediaCornerRadius
                color: MD.Token.color.on_surface
                opacity: !mediaTile.isSkeleton && mediaTile.imageReady
                         ? (mediaTile.isTrailer
                            ? (mediaHover.hovered ? 0.12 : 0.04)
                            : (mediaHover.hovered ? 0.12 : 0))
                         : 0
                Behavior on opacity {
                    NumberAnimation { duration: 150 }
                }
            }

            MD.Icon {
                anchors.centerIn: parent
                visible: !mediaTile.isSkeleton && mediaTile.imageReady
                         && (mediaTile.isTrailer || mediaHover.hovered)
                name: mediaTile.isTrailer ? MD.Token.icon.play_arrow : MD.Token.icon.open_in_new
                size: mediaTile.isTrailer ? 40 : 28
                color: MD.Token.color.on_surface
                opacity: mediaTile.isTrailer ? (mediaHover.hovered ? 1 : 0.92) : 1
            }

            HoverHandler {
                id: mediaHover
                enabled: !mediaTile.isSkeleton && mediaTile.imageReady
            }

            MouseArea {
                anchors.fill: parent
                enabled: !mediaTile.isSkeleton && mediaTile.imageReady
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (mediaTile.isTrailer)
                        root.openTrailerPreview()
                    else
                        root.openScreenshotPreview(modelData.shotIndex)
                }
            }
        }
    }

    Popup {
        id: trailerPreview

        parent: Overlay.overlay
        modal: true
        padding: 0
        anchors.centerIn: parent
        width: parent ? parent.width : 960
        height: parent ? parent.height : 640
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        readonly property var dialogPlayer: trailerPlayerLoader.item?.player

        onOpened: trailerPlayerLoader.active = true
        onClosed: {
            const player = trailerPlayerLoader.item?.player
            if (player && player.playbackState !== MediaPlayer.StoppedState)
                player.stop()
            trailerPlayerLoader.active = false
        }

        background: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.88)
        }

        contentItem: ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.topMargin: MD.Token.spacing.medium
                Layout.bottomMargin: MD.Token.spacing.small

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Gameplay video")
                    typescale: MD.Token.typescale.title_medium
                    color: Qt.rgba(1, 1, 1, 0.92)
                }

                MD.IconButton {
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.close
                    onClicked: trailerPreview.close()
                }
            }

            Item {
                id: videoFrame
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 280
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.topMargin: MD.Token.spacing.small
                Layout.bottomMargin: MD.Token.spacing.small
                clip: true

                layer.enabled: true
                layer.effect: MD.RoundClip {
                    corners: MD.Util.corners(root.mediaCornerRadius)
                    size: Qt.vector2d(videoFrame.width, videoFrame.height)
                }

                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(0, 0, 0, 0.55)
                }

                Loader {
                    id: trailerPlayerLoader
                    anchors.fill: parent
                    active: false

                    sourceComponent: Component {
                        Item {
                            id: trailerPlayerRoot
                            anchors.fill: parent

                            property alias player: trailerDialogPlayer

                            VideoOutput {
                                id: trailerVideoOutput
                                anchors.fill: parent
                                fillMode: VideoOutput.PreserveAspectFit
                            }

                            MediaPlayer {
                                id: trailerDialogPlayer
                                videoOutput: trailerVideoOutput
                                audioOutput: trailerDialogAudio
                                source: root.trailerUrl
                                loops: MediaPlayer.Infinite
                                autoPlay: true

                                onErrorOccurred: (error, errorString) => {
                                    console.warn("Trailer playback failed:", errorString,
                                                 "url:", root.trailerUrl)
                                }
                            }

                            AudioOutput {
                                id: trailerDialogAudio
                                volume: 0.85
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: MD.Token.color.on_surface
                                opacity: trailerDialogHover.hovered ? 0.08 : 0
                                Behavior on opacity {
                                    NumberAnimation { duration: 150 }
                                }
                            }

                            HoverHandler { id: trailerDialogHover }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.toggleTrailerPlayback()
                            }

                            MD.IconButton {
                                anchors.centerIn: parent
                                visible: trailerDialogPlayer.playbackState !== MediaPlayer.PlayingState
                                mdState.type: MD.Enum.IBtFilled
                                icon.name: MD.Token.icon.play_arrow
                                onClicked: root.toggleTrailerPlayback()
                            }

                            Component.onDestruction: {
                                if (trailerDialogPlayer.playbackState !== MediaPlayer.StoppedState)
                                    trailerDialogPlayer.stop()
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.bottomMargin: MD.Token.spacing.large
                spacing: MD.Token.spacing.small

                MD.Button {
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Close")
                    onClicked: trailerPreview.close()
                }

                MD.IconButton {
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: trailerPreview.dialogPlayer
                               && trailerPreview.dialogPlayer.playbackState === MediaPlayer.PlayingState
                               ? MD.Token.icon.pause
                               : MD.Token.icon.play_arrow
                    onClicked: root.toggleTrailerPlayback()
                }

                Item { Layout.fillWidth: true }

                MD.Button {
                    visible: root.trailerUrl.length > 0
                    mdState.type: MD.Enum.BtOutlined
                    text: qsTr("Open in browser")
                    icon.name: MD.Token.icon.open_in_new
                    onClicked: Core.openExternalUrl(root.trailerUrl)
                }
            }
        }
    }

    Popup {
        id: screenshotPreview

        parent: Overlay.overlay
        modal: true
        padding: 0
        anchors.centerIn: parent
        width: parent ? parent.width : 960
        height: parent ? parent.height : 640
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property int currentIndex: 0

        readonly property var urls: root.screenshotUrls ?? []
        readonly property string currentUrl: urls.length > 0 && currentIndex >= 0
                                         && currentIndex < urls.length
                                         ? urls[currentIndex]
                                         : ""

        onClosed: screenshotPreview.currentIndex = 0

        background: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.88)
        }

        contentItem: ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.topMargin: MD.Token.spacing.medium
                Layout.bottomMargin: MD.Token.spacing.small

                MD.Label {
                    Layout.fillWidth: true
                    text: screenshotPreview.urls.length > 0
                          ? qsTr("Screenshot %1 of %2")
                                .arg(screenshotPreview.currentIndex + 1)
                                .arg(screenshotPreview.urls.length)
                          : qsTr("Screenshots")
                    typescale: MD.Token.typescale.title_medium
                    color: Qt.rgba(1, 1, 1, 0.92)
                }

                MD.IconButton {
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.close
                    onClicked: screenshotPreview.close()
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 280
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large

                MD.Image {
                    id: previewImage
                    anchors.fill: parent
                    anchors.topMargin: MD.Token.spacing.small
                    anchors.bottomMargin: MD.Token.spacing.small
                    source: screenshotPreview.currentUrl
                    radius: root.mediaCornerRadius
                    elevation: MD.Token.elevation.level0
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }

                MD.IconButton {
                    visible: screenshotPreview.urls.length > 1
                             && screenshotPreview.currentIndex > 0
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    mdState.type: MD.Enum.IBtFilled
                    icon.name: MD.Token.icon.arrow_back
                    onClicked: screenshotPreview.currentIndex--
                }

                MD.IconButton {
                    visible: screenshotPreview.urls.length > 1
                             && screenshotPreview.currentIndex < screenshotPreview.urls.length - 1
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    mdState.type: MD.Enum.IBtFilled
                    icon.name: MD.Token.icon.chevron_right
                    onClicked: screenshotPreview.currentIndex++
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                Layout.bottomMargin: MD.Token.spacing.medium
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                visible: screenshotPreview.urls.length > 1
                orientation: ListView.Horizontal
                spacing: MD.Token.spacing.small
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: screenshotPreview.urls

                delegate: Item {
                    required property int index
                    required property var modelData

                    width: 112
                    height: 63

                    MD.Image {
                        anchors.fill: parent
                        source: modelData
                        radius: MD.Token.shape.corner.small
                        elevation: MD.Token.elevation.level0
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: MD.Token.shape.corner.small
                        color: "transparent"
                        border.width: screenshotPreview.currentIndex === index ? 2 : 0
                        border.color: MD.Token.color.primary
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: screenshotPreview.currentIndex = index
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.bottomMargin: MD.Token.spacing.large
                spacing: MD.Token.spacing.small

                MD.Button {
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Close")
                    onClicked: screenshotPreview.close()
                }

                Item { Layout.fillWidth: true }

                MD.Button {
                    visible: screenshotPreview.currentUrl.length > 0
                    mdState.type: MD.Enum.BtOutlined
                    text: qsTr("Open in browser")
                    icon.name: MD.Token.icon.open_in_new
                    onClicked: Core.openExternalUrl(screenshotPreview.currentUrl)
                }
            }
        }
    }
}
