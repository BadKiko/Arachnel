import QtQuick
import Qcm.Material as MD

MD.LinearIndicator {
    id: root

    required property var page

    // Active / installing only - no track on completed (looks like a stray underline).
    readonly property bool showTrack: page.inProgress || page.isInstalling

    visible: showTrack
    implicitHeight: visible ? (page.addonRow ? 4 : 5) : 0
    strokeWidth: implicitHeight
    indeterminate: page.isInstalling
    running: page.isInstalling && root.visible
    from: 0
    to: 100
    value: page.isInstalling ? 0 : Math.max(0, Math.min(100, page.progress || 0))
    color: page.isPaused ? MD.Token.color.on_surface_variant
                         : (page.isFailed ? MD.Token.color.error : MD.Token.color.primary)
    trackColor: MD.Util.transparent(root.color, 0.18)
}
