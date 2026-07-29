import QtQuick

import Qcm.Material as MD

// Slim journey scrub. Hold still briefly for one precision window — rail
// visuals stay stable; feedback is in the HUD only.
Item {
    id: root

    signal jumpToRow(int row)
    signal jumpToEdge(int edge) // 0 = top, 1 = bottom

    property var stops: []
    property int catalogCount: 0
    property int currentRow: 0
    property real viewportFraction: 0.02

    readonly property int stopCount: Array.isArray(root.stops) ? root.stops.length : 0
    readonly property bool scrubbing: trackPad.pressed
    property bool precision: false
    property bool dragFromThumb: false
    property string previewLabel: ""
    property int previewRow: -1

    property int anchorRow: 0
    property real anchorY: 0
    property real pressY: 0
    property bool pressMoved: false

    implicitWidth: 22
    width: implicitWidth
    visible: root.catalogCount >= 32

    // Precision: slower scrub (~2% of catalog per full rail travel).
    readonly property int zoomWindow: Math.max(48, Math.round(root.catalogCount * 0.02))

    // Always global position — remapping to a local window made the thumb
    // jump to the center when fine scrub engaged.
    readonly property real progress: {
        const n = Math.max(1, root.catalogCount - 1)
        const row = root.scrubbing && root.previewRow >= 0 ? root.previewRow : root.currentRow
        return Math.max(0, Math.min(1, row / n))
    }

    function resetPrecision() {
        precisionArm.stop()
        root.precision = false
        root.pressMoved = false
        root.dragFromThumb = false
    }

    function thumbHit(y) {
        const top = track.y + thumb.y - 3
        const bottom = track.y + thumb.y + thumb.height + 3
        return y >= top && y <= bottom
    }

    function rowAtY(y) {
        const h = track.height
        if (h <= 0 || root.catalogCount <= 0)
            return 0
        const last = Math.max(0, root.catalogCount - 1)

        // Thumb drag or fine scrub: relative to grab — never absolute-jump.
        if (root.precision) {
            const delta = (y - root.anchorY) / h * root.zoomWindow
            return Math.max(0, Math.min(last, Math.round(root.anchorRow + delta)))
        }
        if (root.dragFromThumb) {
            const travel = Math.max(1, h - thumb.height)
            const delta = (y - root.anchorY) / travel * last
            return Math.max(0, Math.min(last, Math.round(root.anchorRow + delta)))
        }

        const t = Math.max(0, Math.min(1, y / h))
        return Math.round(t * last)
    }

    function nearestStop(row) {
        if (root.stopCount === 0)
            return null
        let best = root.stops[0]
        let bestDist = Math.abs((best.row ?? 0) - row)
        for (let i = 1; i < root.stopCount; ++i) {
            const s = root.stops[i]
            const d = Math.abs((s.row ?? 0) - row)
            if (d < bestDist) {
                best = s
                bestDist = d
            }
        }
        return best
    }

    function labelForRow(row) {
        const stop = nearestStop(row)
        return stop && stop.label ? String(stop.label) : ""
    }

    function scrubToY(y, commit) {
        const row = root.rowAtY(y)
        root.previewRow = row
        root.previewLabel = root.labelForRow(row)
        if (!commit)
            return
        scrubCommit.pendingRow = row
        if (!scrubCommit.running)
            scrubCommit.start()
    }

    function grabThumb(y) {
        root.dragFromThumb = true
        root.anchorRow = root.currentRow
        root.anchorY = y
        root.previewRow = root.currentRow
        root.previewLabel = root.labelForRow(root.currentRow)
    }

    function enterPrecision() {
        if (!trackPad.pressed || root.catalogCount < 64 || root.precision)
            return
        // Lock current place under the cursor — do not re-scrub (avoids jumps).
        root.anchorRow = root.previewRow >= 0 ? root.previewRow : root.currentRow
        root.anchorY = trackPad.mouseY
        root.dragFromThumb = true
        root.precision = true
    }

    Timer {
        id: scrubCommit
        interval: 16
        property int pendingRow: -1
        onTriggered: {
            if (pendingRow >= 0)
                root.jumpToRow(pendingRow)
        }
    }

    Timer {
        id: precisionArm
        interval: 380
        onTriggered: {
            if (trackPad.pressed && !root.pressMoved)
                root.enterPrecision()
        }
    }

    MouseArea {
        id: trackPad
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        preventStealing: true

        onPressed: function (mouse) {
            root.resetPrecision()
            root.pressY = mouse.y
            root.pressMoved = false
            if (root.thumbHit(mouse.y)) {
                // Grab thumb in place — don't absolute-jump under the cursor.
                root.grabThumb(mouse.y)
            } else {
                root.dragFromThumb = false
                root.scrubToY(mouse.y, true)
            }
            precisionArm.restart()
        }
        onPositionChanged: function (mouse) {
            if (pressed) {
                if (Math.abs(mouse.y - root.pressY) > 5)
                    root.pressMoved = true
                if (!root.precision) {
                    if (root.pressMoved)
                        precisionArm.stop()
                    else
                        precisionArm.restart()
                }
                root.scrubToY(mouse.y, true)
                return
            }
            const row = root.rowAtY(mouse.y)
            root.previewRow = row
            root.previewLabel = root.labelForRow(row)
        }
        onReleased: {
            root.resetPrecision()
            root.previewLabel = ""
            root.previewRow = -1
        }
        onCanceled: {
            root.resetPrecision()
            root.previewLabel = ""
            root.previewRow = -1
        }
        onExited: {
            if (!pressed) {
                root.previewLabel = ""
                root.previewRow = -1
            }
        }
        onDoubleClicked: function (mouse) {
            if (mouse.y < height * 0.12)
                root.jumpToEdge(0)
            else if (mouse.y > height * 0.88)
                root.jumpToEdge(1)
        }
    }

    Item {
        id: track
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: MD.Token.spacing.extra_small
        anchors.bottomMargin: MD.Token.spacing.extra_small
        width: root.precision ? 9 : 5

        Behavior on width {
            NumberAnimation {
                duration: MD.Token.duration.short4
                easing: MD.Token.easing.emphasized_decelerate
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: MD.Util.transparent(MD.Token.color.on_surface,
                                       root.scrubbing || trackPad.containsMouse ? 0.18 : 0.10)

            Behavior on color {
                ColorAnimation { duration: MD.Token.duration.short4 }
            }
        }

        Rectangle {
            id: thumb
            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter
            height: root.precision
                    ? Math.max(28, Math.min(52, track.height * 0.08))
                    : Math.max(16, Math.min(32, track.height * Math.max(0.03, root.viewportFraction)))
            radius: width / 2
            y: Math.round(root.progress * Math.max(0, track.height - height))
            color: MD.Token.color.primary
            opacity: {
                if (root.precision)
                    return 1.0
                if (root.scrubbing || trackPad.containsMouse)
                    return 0.92
                return 0.7
            }
            z: 2

            Behavior on y {
                enabled: !root.scrubbing
                NumberAnimation {
                    duration: MD.Token.duration.short4
                    easing: MD.Token.easing.emphasized_decelerate
                }
            }
            Behavior on height {
                NumberAnimation {
                    duration: MD.Token.duration.short4
                    easing: MD.Token.easing.emphasized_decelerate
                }
            }
            Behavior on opacity {
                NumberAnimation {
                    duration: MD.Token.duration.short4
                    easing: MD.Token.easing.standard
                }
            }
        }
    }

    Rectangle {
        id: hud
        readonly property bool shown: trackPad.containsMouse || root.scrubbing
        width: Math.max(100, hudCol.implicitWidth + MD.Token.spacing.small * 2)
        height: hudCol.implicitHeight + MD.Token.spacing.extra_small * 2
        radius: MD.Token.shape.corner.medium
        color: root.precision ? MD.Token.color.primary_container
                              : MD.Token.color.surface_container_high
        border.width: 1
        border.color: MD.Util.transparent(
            root.precision ? MD.Token.color.primary : MD.Token.color.outline_variant, 0.5)
        anchors.right: parent.left
        anchors.rightMargin: MD.Token.spacing.small
        y: {
            const thumbCenter = thumb.y + thumb.height / 2 + track.y
            return Math.max(0, Math.min(root.height - height, thumbCenter - height / 2))
        }
        z: 5
        opacity: shown ? 1 : 0
        visible: opacity > 0.01
        scale: shown ? 1 : 0.94
        transformOrigin: Item.Right

        Behavior on opacity {
            NumberAnimation {
                duration: MD.Token.duration.short4
                easing: MD.Token.easing.emphasized_decelerate
            }
        }
        Behavior on scale {
            NumberAnimation {
                duration: MD.Token.duration.short4
                easing: MD.Token.easing.emphasized_decelerate
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: MD.Token.duration.medium1
                easing: MD.Token.easing.standard
            }
        }
        Behavior on y {
            enabled: root.scrubbing
            NumberAnimation {
                duration: 90
                easing.type: Easing.OutCubic
            }
        }

        Column {
            id: hudCol
            anchors.centerIn: parent
            spacing: 2

            MD.Label {
                id: fineLabel
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Fine scrub")
                typescale: MD.Token.typescale.label_small
                color: MD.Token.color.on_primary_container
                horizontalAlignment: Text.AlignHCenter
                opacity: root.precision ? 1 : 0
                height: root.precision ? implicitHeight : 0
                clip: true

                Behavior on opacity {
                    NumberAnimation {
                        duration: MD.Token.duration.short3
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
                Behavior on height {
                    NumberAnimation {
                        duration: MD.Token.duration.short4
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
            }

            MD.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.previewLabel.length ? root.previewLabel
                                               : root.labelForRow(
                                                     root.previewRow >= 0 ? root.previewRow
                                                                         : root.currentRow)
                typescale: MD.Token.typescale.label_large
                color: root.precision ? MD.Token.color.on_primary_container
                                      : MD.Token.color.on_surface
                horizontalAlignment: Text.AlignHCenter

                Behavior on color {
                    ColorAnimation { duration: MD.Token.duration.short4 }
                }
            }

            MD.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: {
                    const row = root.previewRow >= 0 ? root.previewRow : root.currentRow
                    const shown = Math.min(root.catalogCount, Math.max(1, row + 1))
                    const loc = Qt.locale()
                    return qsTr("%1 of %2").arg(loc.toString(shown, "f", 0))
                                           .arg(loc.toString(root.catalogCount, "f", 0))
                }
                typescale: MD.Token.typescale.label_small
                color: root.precision
                       ? MD.Util.transparent(MD.Token.color.on_primary_container, 0.82)
                       : MD.Token.color.on_surface_variant
                horizontalAlignment: Text.AlignHCenter

                Behavior on color {
                    ColorAnimation { duration: MD.Token.duration.short4 }
                }
            }
        }
    }

    Accessible.name: qsTr("Catalog navigator")
    Accessible.role: Accessible.ScrollBar
}
