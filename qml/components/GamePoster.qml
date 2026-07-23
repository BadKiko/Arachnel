import QtQuick
import QtQuick.Effects

import Qcm.Material as MD

Item {
    id: root

    property url source
    property string seed: ""
    property string fallbackText: ""
    // True while Steam metadata/cover URL is being resolved (before Image has a source).
    property bool awaiting: false
    property int cornerRadius: MD.Token.shape.corner.extra_large
    property bool hovered: mouseArea.containsMouse
    // 0–100: color fills left-to-right over grayscale; <0 disables the effect.
    property real fillProgress: -1

    readonly property int decodeWidth: 300
    readonly property int decodeHeight: 450
    readonly property real fillRatio: Math.max(0, Math.min(1, fillProgress / 100))

    readonly property bool hasSource: source.toString().length > 0
    readonly property bool coverReady: hasSource && coverProbe.status === Image.Ready
    readonly property bool showFillProgress: fillProgress >= 0 && coverReady
    readonly property bool imageLoading: hasSource
                                         && (coverProbe.status === Image.Loading
                                             || coverProbe.status === Image.Null)
    readonly property bool showShimmer: !coverReady && (awaiting || imageLoading)

    readonly property string monogram: {
        const s = (seed || fallbackText || "?").trim()
        if (!s.length)
            return "?"
        const parts = s.split(/\s+/).filter(p => p.length > 0)
        if (parts.length >= 2)
            return (parts[0].charAt(0) + parts[1].charAt(0)).toUpperCase()
        return s.slice(0, Math.min(2, s.length)).toUpperCase()
    }

    signal clicked()
    signal loadFailed()

    implicitWidth: 168
    implicitHeight: Math.round(width * 4 / 3)

    Image {
        id: coverProbe
        visible: false
        source: root.source
        asynchronous: true
        cache: true
        sourceSize.width: root.decodeWidth
        sourceSize.height: root.decodeHeight

        onStatusChanged: {
            if (status === Image.Error && root.hasSource)
                root.loadFailed()
        }
    }

    Item {
        id: placeholder
        anchors.fill: parent
        visible: !root.coverReady
        clip: true

        layer.enabled: true
        layer.effect: MD.RoundClip {
            corners: MD.Util.corners(root.cornerRadius)
            size: Qt.vector2d(placeholder.width, placeholder.height)
        }

        Rectangle {
            anchors.fill: parent
            color: MD.Token.color.surface_container_high
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: MD.Util.transparent(MD.Token.color.primary, 0.08) }
                GradientStop { position: 1.0; color: MD.Util.transparent(MD.Token.color.primary, 0.02) }
            }
        }

        Item {
            id: shimmerClip
            anchors.fill: parent
            visible: root.showShimmer
            clip: true

            Rectangle {
                id: shimmerBand
                width: parent.width * 0.4
                height: parent.height * 1.4
                rotation: 18
                anchors.verticalCenter: parent.verticalCenter
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop {
                        position: 0.5
                        color: MD.Util.transparent(MD.Token.color.on_surface, 0.14)
                    }
                    GradientStop { position: 1.0; color: "transparent" }
                }

                SequentialAnimation on x {
                    running: root.showShimmer && placeholder.visible
                    loops: Animation.Infinite
                    NumberAnimation {
                        from: -shimmerBand.width
                        to: placeholder.width + shimmerBand.width
                        duration: 1400
                        easing.type: Easing.InOutCubic
                    }
                    PauseAnimation { duration: 280 }
                }
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: MD.Token.spacing.small
            visible: !root.showShimmer
            opacity: 0.9

            MD.Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                name: MD.Token.icon.sports_esports
                size: 32
                color: MD.Token.color.on_surface_variant
            }

            MD.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.monogram
                typescale: MD.Token.typescale.title_large
                color: MD.Token.color.on_surface_variant
            }
        }
    }

    MD.Image {
        id: coverImage
        anchors.fill: parent
        source: root.source
        radius: root.cornerRadius
        elevation: MD.Token.elevation.level0
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        smooth: true
        sourceSize.width: root.decodeWidth
        sourceSize.height: root.decodeHeight
        visible: root.coverReady && !root.showFillProgress
        opacity: root.coverReady ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: MD.Token.duration.short4
                easing.type: Easing.OutCubic
            }
        }
    }

    Item {
        id: fillHost
        anchors.fill: parent
        visible: root.showFillProgress
        clip: true

        layer.enabled: true
        layer.effect: MD.RoundClip {
            corners: MD.Util.corners(root.cornerRadius)
            size: Qt.vector2d(fillHost.width, fillHost.height)
        }

        Image {
            id: grayImage
            anchors.fill: parent
            source: root.source
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            smooth: true
            sourceSize.width: root.decodeWidth
            sourceSize.height: root.decodeHeight
            visible: false
        }

        MultiEffect {
            anchors.fill: parent
            source: grayImage
            saturation: -1.0
        }

        Item {
            id: colorClip
            width: parent.width * root.fillRatio
            height: parent.height
            clip: true

            Image {
                width: fillHost.width
                height: fillHost.height
                source: root.source
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                smooth: true
                sourceSize.width: root.decodeWidth
                sourceSize.height: root.decodeHeight
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        visible: root.hovered
        z: 2
        color: MD.Util.transparent(MD.Token.color.scrim, 0.22)
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
