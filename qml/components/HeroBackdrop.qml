import QtQuick
import QtQuick.Effects

import Qcm.Material as MD

// Soft blurred cover behind a page header. Decodes the cover small (it is blurred anyway),
// fades into the page surface at the bottom, and falls back to a plain tonal gradient when
// there is no cover yet or "reduce motion" is on (no blur pass).
Item {
    id: root

    property url source
    property color surfaceColor: MD.Token.color.surface
    property real coverOpacity: 0.55

    readonly property bool blurActive: !AppMotion.reduceMotion
                                       && root.source.toString().length > 0
                                       && cover.status === Image.Ready

    clip: true

    Image {
        id: cover
        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(256, 256)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: cover
        visible: root.blurActive
        opacity: root.blurActive ? root.coverOpacity : 0
        blurEnabled: true
        blur: 1.0
        blurMax: 48
        saturation: 0.35
        autoPaddingEnabled: false

        Behavior on opacity {
            NumberAnimation {
                duration: AppMotion.long
                easing: AppMotion.standard
            }
        }
    }

    // Tonal wash: the whole look when there is no blur, a tint over the cover otherwise.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: MD.Util.transparent(MD.Token.color.primary, root.blurActive ? 0.06 : 0.16)
            }
            GradientStop {
                position: 1.0
                color: "transparent"
            }
        }
    }

    // Blend into the page.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.55; color: MD.Util.transparent(root.surfaceColor, 0.55) }
            GradientStop { position: 1.0; color: root.surfaceColor }
        }
    }
}
