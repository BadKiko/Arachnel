import QtQuick
import Qcm.Material as MD

MD.LinearIndicator {
    id: root

    required property var page

    // Active / installing only - no track on completed (looks like a stray underline).
    readonly property bool showTrack: page.inProgress || page.isInstalling

    // M3 Expressive: a live transfer gets a wavy active segment. Paused, failed, installing
    // and "reduce motion" fall back to the flat bar (the wave animates continuously).
    readonly property bool wavyTrack: showTrack && page.inProgress && !page.isPaused
                                      && !page.isFailed && !page.isInstalling
                                      && !AppMotion.reduceMotion
    readonly property int barThickness: page.addonRow ? 4 : 5

    visible: showTrack
    wavy: wavyTrack
    waveAmplitude: 2
    waveLength: 26
    // The wave needs vertical room on both sides of the centerline.
    implicitHeight: visible ? (wavyTrack ? barThickness + 2 * waveAmplitude + 2 : barThickness) : 0
    strokeWidth: barThickness
    indeterminate: page.isInstalling
    running: page.isInstalling && root.visible
    from: 0
    to: 100
    value: page.isInstalling ? 0 : Math.max(0, Math.min(100, page.progress || 0))
    color: page.isPaused ? MD.Token.color.on_surface_variant
                         : (page.isFailed ? MD.Token.color.error : MD.Token.color.primary)
    trackColor: MD.Util.transparent(root.color, 0.18)
}
