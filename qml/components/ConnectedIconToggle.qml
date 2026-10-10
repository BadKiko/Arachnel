import QtQuick

import Qcm.Material as MD

// Material 3 Expressive connected button group for a single choice (e.g. grid / list). The
// segments touch with a 2 px gap; the chosen one fills in and its inner corners round out with a
// springy overshoot, the others keep tight inner corners.
Row {
    id: root

    property var icons: []
    property var tips: []
    property int currentIndex: 0

    signal picked(int index)

    spacing: 2

    Repeater {
        model: root.icons

        delegate: Item {
            id: seg
            required property var modelData
            required property int index
            readonly property bool chosen: root.currentIndex === index
            readonly property bool firstSeg: index === 0
            readonly property bool lastSeg: index === root.icons.length - 1

            width: chosen ? 56 : 48
            height: 40

            Behavior on width {
                NumberAnimation { duration: AppMotion.scaled(380); easing.type: Easing.OutBack; easing.overshoot: 1.8 }
            }

            readonly property real outer: height / 2
            readonly property real inner: chosen ? height / 2 : (tap.pressed ? 12 : 6)

            Rectangle {
                anchors.fill: parent
                color: seg.chosen ? MD.Token.color.secondary_container
                     : (hover.hovered ? MD.Token.color.surface_container_highest : MD.Token.color.surface_container_high)
                topLeftRadius: seg.firstSeg ? seg.outer : seg.inner
                bottomLeftRadius: seg.firstSeg ? seg.outer : seg.inner
                topRightRadius: seg.lastSeg ? seg.outer : seg.inner
                bottomRightRadius: seg.lastSeg ? seg.outer : seg.inner

                Behavior on color { ColorAnimation { duration: AppMotion.short } }
                Behavior on topLeftRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
                Behavior on bottomLeftRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
                Behavior on topRightRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
                Behavior on bottomRightRadius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
            }

            MD.Icon {
                anchors.centerIn: parent
                name: seg.modelData
                size: 22
                color: seg.chosen ? MD.Token.color.on_secondary_container : MD.Token.color.on_surface_variant
            }

            HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
            TapHandler {
                id: tap
                onTapped: root.picked(seg.index)
            }

            MD.ToolTip {
                visible: hover.hovered && (root.tips[seg.index] ?? "").length > 0
                text: root.tips[seg.index] ?? ""
            }
        }
    }
}
