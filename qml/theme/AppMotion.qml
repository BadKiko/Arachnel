pragma Singleton

import QtQuick
import Qcm.Material as MD

// Single source of motion timing for the app. Thin wrapper over the Material 3 tokens:
// every animation takes its duration and curve from here, so "reduce motion" and any
// future retuning are one-file changes.
QtObject {
    id: root

    // Bound to the Appearance setting; durations collapse to 0 when it is on.
    readonly property bool reduceMotion: Appearance.reduceMotion

    function scaled(ms) {
        return root.reduceMotion ? 0 : ms
    }

    // Durations (ms)
    readonly property int instant: 0
    readonly property int fast: scaled(MD.Token.duration.short2)    // 100: hover, press
    readonly property int quick: scaled(MD.Token.duration.short3)   // 150: small state changes
    readonly property int short: scaled(MD.Token.duration.short4)   // 200: fades, color swaps
    readonly property int medium: scaled(MD.Token.duration.medium2) // 300: panels, shelves
    readonly property int long: scaled(MD.Token.duration.medium4)   // 400: page enter
    readonly property int extraLong: scaled(MD.Token.duration.long2) // 500: hero, large moves

    // Curves
    readonly property var standard: MD.Token.easing.standard
    readonly property var standardDecelerate: MD.Token.easing.standard_decelerate
    readonly property var standardAccelerate: MD.Token.easing.standard_accelerate
    readonly property var emphasized: MD.Token.easing.emphasized
    readonly property var emphasizedDecelerate: MD.Token.easing.emphasized_decelerate
    readonly property var emphasizedAccelerate: MD.Token.easing.emphasized_accelerate

    // List/grid add transitions: cap the stagger so long lists don't crawl.
    readonly property int staggerStep: scaled(30)
    readonly property int staggerMaxItems: 8

    function stagger(index) {
        return Math.min(Math.max(index, 0), root.staggerMaxItems) * root.staggerStep
    }
}
