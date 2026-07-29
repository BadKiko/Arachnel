import QtQuick
import QtQuick.Controls

import Qcm.Material as MD

// Steam-style side preview beside the card. Only opens after a frame that
// belongs to the current entryId is Ready - avoids cross-card screenshot leaks.
Popup {
    id: root

    property string entryId: ""
    property var urls: []
    property bool active: false
    property Item anchorItem: null
    property int advanceMs: 2000
    readonly property bool popupHovered: false

    readonly property var shotList: {
        const raw = root.urls
        if (!raw || !raw.length)
            return []
        const out = []
        const n = Math.min(6, raw.length)
        for (let i = 0; i < n; ++i) {
            const u = String(raw[i] || "")
            if (u.length)
                out.push(u)
        }
        return out
    }

    readonly property int count: root.shotList.length
    property int currentIndex: 0
    property string readyUrl: ""
    property bool frameReady: false
    property int generation: 0
    property int slideDir: 1
    property int lastIndex: 0

    parent: Overlay.overlay
    modal: false
    dim: false
    focus: false
    padding: 0
    closePolicy: Popup.NoAutoClose
    width: 392
    height: 228
    z: 900
    enabled: false

    background: MD.ElevationRectangle {
        radius: MD.Token.shape.corner.large
        color: MD.Token.color.surface_container_high
        elevation: MD.Token.elevation.level2
        // Hide chrome until a real frame is present (avoids empty gray slabs).
        opacity: root.frameReady && shownShot.status === Image.Ready ? 1 : 0
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: MD.Token.duration.short4
            easing: MD.Token.easing.emphasized_decelerate
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            to: 0
            duration: MD.Token.duration.short3
            easing: MD.Token.easing.emphasized_accelerate
        }
    }

    function urlBelongs(url) {
        if (!url || !url.length)
            return false
        const list = root.shotList
        for (let i = 0; i < list.length; ++i) {
            if (list[i] === url)
                return true
        }
        return false
    }

    function resetLayers() {
        pageAnim.stop()
        shownShot.source = ""
        nextShot.source = ""
        shownShot.opacity = 1
        nextShot.opacity = 0
        shownShot.x = 0
        nextShot.x = 0
        shownShot.scale = 1
        nextShot.scale = 1
    }

    function resetFrame() {
        root.generation += 1
        root.frameReady = false
        root.readyUrl = ""
        root.currentIndex = 0
        root.lastIndex = 0
        root.slideDir = 1
        skipBadFrame.stop()
        advanceTimer.stop()
        advanceGrace.stop()
        root.resetLayers()
        // Drop decoder so a cached Ready from the previous game cannot leak.
        currentShot.source = ""
        preloadShot.source = ""
    }

    function upcomingUrl() {
        if (root.count < 2)
            return ""
        return root.shotList[(root.currentIndex + 1) % root.count] || ""
    }

    function warmNextLayer() {
        if (pageAnim.running || nextShot.opacity > 0.01)
            return
        const u = root.upcomingUrl()
        if (!u.length || preloadShot.status !== Image.Ready)
            return
        if (preloadShot.source.toString() !== u)
            return
        if (nextShot.source.toString() !== u)
            nextShot.source = u
    }

    function preloadNext() {
        if (!root.active || root.count < 2) {
            preloadShot.source = ""
            return
        }
        const u = root.upcomingUrl()
        if (!u.length) {
            preloadShot.source = ""
            return
        }
        if (preloadShot.source.toString() !== u)
            preloadShot.source = u
        else if (preloadShot.status === Image.Ready)
            root.warmNextLayer()
    }

    // Crossfade + soft parallax slide between already-decoded frames.
    function presentFrame(url) {
        if (!root.urlBelongs(url))
            return
        if (pageAnim.running)
            pageAnim.stop()

        const current = shownShot.source.toString()
        if (!current.length || current === url || !root.opened) {
            shownShot.source = url
            shownShot.opacity = 1
            shownShot.x = 0
            shownShot.scale = 1
            nextShot.source = ""
            nextShot.opacity = 0
            nextShot.x = 0
            nextShot.scale = 1
            Qt.callLater(root.preloadNext)
            return
        }

        const dir = root.slideDir
        const travel = Math.round(root.width * 0.12)
        nextShot.source = url
        nextShot.opacity = 0
        nextShot.x = dir * travel
        nextShot.scale = 1.06
        shownShot.opacity = 1
        shownShot.x = 0
        shownShot.scale = 1
        pageAnim.dir = dir
        pageAnim.travel = travel
        pageAnim.start()
    }

    function advancePage() {
        if (!root.active || root.count < 2)
            return
        advanceGrace.stop()
        root.currentIndex = (root.currentIndex + 1) % root.count
    }

    function tryAdvance() {
        if (!root.active || root.count < 2 || !root.opened)
            return
        const u = root.upcomingUrl()
        if (u.length && preloadShot.source.toString() === u
                && preloadShot.status === Image.Loading) {
            // Wait for the next frame, but don't stall forever.
            advanceGrace.restart()
            return
        }
        root.advancePage()
    }

    function reposition() {
        if (!root.anchorItem || !Overlay.overlay)
            return
        const gap = 12
        const overlay = Overlay.overlay
        const p = root.anchorItem.mapToItem(overlay, 0, 0)
        const rightX = p.x + root.anchorItem.width + gap
        const leftX = p.x - root.width - gap
        let nx
        if (rightX + root.width <= overlay.width - gap)
            nx = rightX
        else if (leftX >= gap)
            nx = leftX
        else
            nx = Math.max(gap, Math.min(p.x + root.anchorItem.width + gap,
                                        overlay.width - root.width - gap))

        let ny = p.y + (root.anchorItem.height - root.height) / 2
        ny = Math.max(gap, Math.min(ny, overlay.height - root.height - gap))
        root.x = nx
        root.y = ny
    }

    function showWhenReady() {
        if (!root.active || !root.frameReady || root.count === 0)
            return
        if (!root.urlBelongs(root.readyUrl))
            return
        if (!root.anchorVisible())
            return
        root.reposition()
        root.presentFrame(root.readyUrl)
        if (!root.opened)
            root.open()
        root.preloadNext()
    }

    function anchorVisible() {
        let p = root.anchorItem
        if (!p)
            return false
        while (p) {
            if (p.visible === false)
                return false
            p = p.parent
        }
        return true
    }

    function hideNow() {
        advanceTimer.stop()
        advanceGrace.stop()
        skipBadFrame.stop()
        pageAnim.stop()
        // Close first while the frame is still painted — resetting images
        // during the exit transition left empty gray popup ghosts on Overlay.
        if (root.opened) {
            root.close()
        } else {
            root.resetFrame()
        }
    }

    onClosed: root.resetFrame()

    // Drop orphans if the card was hidden/reused while the popup stayed on Overlay.
    Timer {
        interval: 180
        running: root.opened
        repeat: true
        onTriggered: {
            if (!root.active || !root.anchorVisible())
                root.hideNow()
        }
    }

    function bindDecoder() {
        if (!root.active || root.count === 0) {
            currentShot.source = ""
            return
        }
        const next = root.shotList[root.currentIndex] || ""
        if (currentShot.source.toString() === next) {
            if (currentShot.status === Image.Ready && root.urlBelongs(next)) {
                root.readyUrl = next
                root.frameReady = true
                root.showWhenReady()
            }
            return
        }
        currentShot.source = next
    }

    function applyIndexFrame() {
        if (!root.active)
            return
        const next = root.shotList[root.currentIndex] || ""
        // Fast path: next frame already decoded by the preloader.
        if (next.length
                && preloadShot.source.toString() === next
                && preloadShot.status === Image.Ready
                && root.urlBelongs(next)) {
            currentShot.source = next
            root.readyUrl = next
            root.frameReady = true
            root.showWhenReady()
            return
        }
        if (!root.urlBelongs(root.readyUrl)) {
            root.frameReady = false
            root.readyUrl = ""
        }
        root.bindDecoder()
    }

    onEntryIdChanged: hideNow()

    onUrlsChanged: {
        if (!root.active) {
            if (root.opened)
                root.close()
            else
                root.resetFrame()
            return
        }
        root.generation += 1
        root.frameReady = false
        root.readyUrl = ""
        root.currentIndex = 0
        root.lastIndex = 0
        root.slideDir = 1
        skipBadFrame.stop()
        advanceTimer.stop()
        advanceGrace.stop()
        // Keep the current painted frame until the next Ready to avoid a gray flash.
        currentShot.source = ""
        preloadShot.source = ""
        Qt.callLater(root.bindDecoder)
    }

    onActiveChanged: {
        if (active) {
            root.resetFrame()
            Qt.callLater(root.bindDecoder)
        } else {
            hideNow()
        }
    }

    onCurrentIndexChanged: {
        if (!root.active)
            return
        const n = root.count
        if (n > 1) {
            const prev = root.lastIndex
            const cur = root.currentIndex
            // Prefer shortest wrap direction for the slide.
            const forward = (cur - prev + n) % n
            const backward = (prev - cur + n) % n
            root.slideDir = forward <= backward ? 1 : -1
        } else {
            root.slideDir = 1
        }
        root.lastIndex = root.currentIndex
        root.applyIndexFrame()
    }

    Timer {
        id: advanceTimer
        interval: root.advanceMs
        repeat: true
        running: root.opened && root.count > 1 && root.frameReady
        onTriggered: root.tryAdvance()
    }

    Timer {
        id: advanceGrace
        interval: 900
        onTriggered: root.advancePage()
    }

    Timer {
        id: skipBadFrame
        interval: 700
        onTriggered: {
            if (root.count > 1)
                root.advancePage()
        }
    }

    Image {
        id: currentShot
        width: 1
        height: 1
        visible: false
        asynchronous: true
        cache: true
        sourceSize.width: 800
        sourceSize.height: 450

        onStatusChanged: {
            if (!root.active)
                return
            const u = source.toString()
            if (status === Image.Ready) {
                if (!root.urlBelongs(u))
                    return
                root.readyUrl = u
                root.frameReady = true
                skipBadFrame.stop()
                root.showWhenReady()
            } else if (status === Image.Error) {
                if (!root.urlBelongs(root.readyUrl)) {
                    root.frameReady = false
                    root.readyUrl = ""
                }
                skipBadFrame.restart()
            } else if (status === Image.Loading) {
                skipBadFrame.restart()
            }
        }
    }

    // Invisible decoder for the upcoming carousel frame.
    Image {
        id: preloadShot
        width: 1
        height: 1
        visible: false
        asynchronous: true
        cache: true
        sourceSize.width: 800
        sourceSize.height: 450

        onStatusChanged: {
            if (!root.active)
                return
            const u = source.toString()
            if (status === Image.Ready) {
                if (u !== root.upcomingUrl() || !root.urlBelongs(u))
                    return
                root.warmNextLayer()
                // If the carousel is waiting on this frame, advance now.
                if (advanceGrace.running)
                    root.advancePage()
            } else if (status === Image.Error) {
                if (advanceGrace.running)
                    root.advancePage()
            }
        }
    }

    ParallelAnimation {
        id: pageAnim
        property int dir: 1
        property real travel: 40

        NumberAnimation {
            target: shownShot
            property: "opacity"
            to: 0
            duration: 440
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: shownShot
            property: "x"
            to: -pageAnim.dir * pageAnim.travel
            duration: 520
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: shownShot
            property: "scale"
            to: 0.94
            duration: 520
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: nextShot
            property: "opacity"
            to: 1
            duration: 380
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: nextShot
            property: "x"
            to: 0
            duration: 520
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: nextShot
            property: "scale"
            to: 1
            duration: 520
            easing.type: Easing.OutCubic
        }

        onFinished: {
            shownShot.source = nextShot.source
            shownShot.opacity = 1
            shownShot.x = 0
            shownShot.scale = 1
            nextShot.source = ""
            nextShot.opacity = 0
            nextShot.x = 0
            nextShot.scale = 1
            root.preloadNext()
        }
    }

    contentItem: Item {
        implicitWidth: root.width
        implicitHeight: root.height
        clip: true

        Rectangle {
            anchors.fill: parent
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container_highest
            clip: true

            layer.enabled: true
            layer.effect: MD.RoundClip {
                corners: MD.Util.corners(MD.Token.shape.corner.large)
                size: Qt.vector2d(width, height)
            }

            Image {
                id: shownShot
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: parent.height
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                smooth: true
                sourceSize.width: 800
                sourceSize.height: 450
                visible: status === Image.Ready
                transformOrigin: Item.Center
            }

            Image {
                id: nextShot
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: parent.height
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                smooth: true
                sourceSize.width: 800
                sourceSize.height: 450
                opacity: 0
                visible: status === Image.Ready || opacity > 0.01
                transformOrigin: Item.Center
            }

            // Accent pill - primary, not black scrim (unreadable on dark shots).
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: MD.Token.spacing.small
                visible: root.count > 1 && root.frameReady
                height: 18
                width: dotsRow.width + 14
                radius: height / 2
                color: MD.Util.transparent(MD.Token.color.primary_container, 0.94)
                border.width: 1
                border.color: MD.Util.transparent(MD.Token.color.primary, 0.55)
                z: 2

                Row {
                    id: dotsRow
                    anchors.centerIn: parent
                    spacing: 4

                    Repeater {
                        model: root.count

                        Rectangle {
                            required property int index
                            width: index === root.currentIndex ? 10 : 5
                            height: 5
                            radius: 2.5
                            color: index === root.currentIndex
                                   ? MD.Token.color.primary
                                   : MD.Util.transparent(MD.Token.color.on_primary_container, 0.45)

                            Behavior on width {
                                NumberAnimation {
                                    duration: MD.Token.duration.short3
                                    easing: MD.Token.easing.emphasized_decelerate
                                }
                            }
                            Behavior on color {
                                ColorAnimation {
                                    duration: MD.Token.duration.short3
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
