import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtMultimedia

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property var screenshotUrls
    required property string trailerUrl
    property real mediaCornerRadius: MD.Token.shape.corner.large

    function openScreenshotPreview(index) {
        if (!root.screenshotUrls || !root.screenshotUrls.length)
            return
        screenshotPreview.currentIndex = Math.max(0, Math.min(index, root.screenshotUrls.length - 1))
        screenshotPreview.open()
    }

    function openTrailerPreview() {
        if (root.trailerUrl.length)
            trailerPreview.open()
    }

    function formatTime(ms) {
        const total = Math.max(0, Math.floor((ms || 0) / 1000))
        const m = Math.floor(total / 60)
        const s = total % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    function seekTrailerBy(deltaMs) {
        const player = trailerPlayerLoader.item?.player
        if (!player || !player.seekable)
            return
        player.position = Math.max(0, Math.min(player.duration, player.position + deltaMs))
    }

    function toggleTrailerMute() {
        const audio = trailerPlayerLoader.item?.audio
        if (audio)
            audio.muted = !audio.muted
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

    Popup {
        id: trailerPreview

        parent: Overlay.overlay
        modal: true
        padding: 0
        anchors.centerIn: parent
        width: parent ? parent.width : 960
        height: parent ? parent.height : 640
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        focus: true

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
            // Keys can't be attached to a Popup, so the content item takes the focus.
            focus: true

            Keys.onSpacePressed: root.toggleTrailerPlayback()
            Keys.onLeftPressed: root.seekTrailerBy(-5000)
            Keys.onRightPressed: root.seekTrailerBy(5000)
            Keys.onPressed: function (event) {
                if (event.key === Qt.Key_M) {
                    root.toggleTrailerMute()
                    event.accepted = true
                }
            }

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
                            property alias audio: trailerDialogAudio

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
                                    NumberAnimation { duration: AppMotion.quick }
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
                spacing: MD.Token.spacing.small

                MD.Label {
                    Layout.preferredWidth: 44
                    horizontalAlignment: Text.AlignRight
                    text: root.formatTime(trailerPreview.dialogPlayer ? trailerPreview.dialogPlayer.position : 0)
                    color: Qt.rgba(1, 1, 1, 0.8)
                    typescale: MD.Token.typescale.label_medium
                }

                MD.Slider {
                    id: seekSlider
                    Layout.fillWidth: true
                    focusPolicy: Qt.NoFocus
                    from: 0
                    to: Math.max(1, trailerPreview.dialogPlayer ? trailerPreview.dialogPlayer.duration : 1)
                    enabled: !!trailerPreview.dialogPlayer && trailerPreview.dialogPlayer.seekable
                    onMoved: {
                        if (trailerPreview.dialogPlayer)
                            trailerPreview.dialogPlayer.position = value
                    }

                    // A plain binding would be dropped on the first drag.
                    Binding {
                        target: seekSlider
                        property: "value"
                        value: trailerPreview.dialogPlayer ? trailerPreview.dialogPlayer.position : 0
                        when: !seekSlider.pressed
                    }
                }

                MD.Label {
                    Layout.preferredWidth: 44
                    text: root.formatTime(trailerPreview.dialogPlayer ? trailerPreview.dialogPlayer.duration : 0)
                    color: Qt.rgba(1, 1, 1, 0.8)
                    typescale: MD.Token.typescale.label_medium
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

                MD.IconButton {
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: trailerPlayerLoader.item && trailerPlayerLoader.item.audio.muted
                               ? MD.Token.icon.volume_off
                               : MD.Token.icon.volume_up
                    onClicked: root.toggleTrailerMute()
                }

                MD.Label {
                    text: qsTr("Space - pause, ← → - seek, M - mute")
                    color: Qt.rgba(1, 1, 1, 0.55)
                    typescale: MD.Token.typescale.label_small
                    visible: parent.width > 640
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
        focus: true

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
            focus: true

            Keys.onLeftPressed: if (screenshotPreview.currentIndex > 0) screenshotPreview.currentIndex--
            Keys.onRightPressed: if (screenshotPreview.currentIndex < screenshotPreview.urls.length - 1)
                                     screenshotPreview.currentIndex++
            Keys.onPressed: function (event) {
                if (event.key === Qt.Key_Home) {
                    screenshotPreview.currentIndex = 0
                    event.accepted = true
                } else if (event.key === Qt.Key_End) {
                    screenshotPreview.currentIndex = Math.max(0, screenshotPreview.urls.length - 1)
                    event.accepted = true
                }
            }

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
                    icon.name: MD.Token.icon.chevron_left
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