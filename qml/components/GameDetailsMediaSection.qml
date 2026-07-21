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
        mediaPreview.openScreenshotPreview(clamped)
    }

    function openTrailerPreview() {
        if (!root.hasTrailer)
            return
        mediaPreview.openTrailerPreview()
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

    GameDetailsMediaPreview {
        id: mediaPreview
        screenshotUrls: root.screenshotUrls
        trailerUrl: root.trailerUrl
        mediaCornerRadius: root.mediaCornerRadius
    }
}