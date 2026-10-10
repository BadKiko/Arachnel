import QtQuick
import QtQuick.Controls

import Qcm.Material as MD

// Launcher v2: settings slide in as a modal side sheet from the right edge, full height, instead
// of a bottom sheet. Same open*() API as before.
Popup {
    id: root

    modal: true
    dim: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    parent: Overlay.overlay
    width: Math.min(600, (parent ? parent.width : 800) - 96)
    height: parent ? parent.height - 16 : 600
    x: parent ? parent.width - width - 8 : 0
    y: 8

    onAboutToShow: settingsPage.syncFromStore()
    onClosed: settingsPage.resetOnClose()

    function openSettings() {
        settingsPage.prepareOpen("", false)
        open()
    }

    function openPlugins() {
        settingsPage.prepareOpen("plugins", false)
        open()
    }

    function openSources(createSource) {
        settingsPage.prepareOpen("sources", !!createSource)
        open()
    }

    function openFriends() {
        settingsPage.prepareOpen("friends", false)
        open()
    }

    function openLaunch() {
        if (Qt.platform.os !== "linux")
            return
        settingsPage.prepareOpen("launch", false)
        open()
    }

    Overlay.modal: Rectangle {
        color: MD.Util.transparent(MD.Token.color.scrim, 0.42)
    }

    background: Rectangle {
        radius: 28
        color: MD.Token.color.surface_container_low
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "x"; from: root.parent ? root.parent.width : 0; duration: AppMotion.scaled(480); easing: AppMotion.emphasizedDecelerate }
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppMotion.short }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "x"; to: root.parent ? root.parent.width : 0; duration: AppMotion.scaled(300); easing: AppMotion.emphasizedAccelerate }
            NumberAnimation { property: "opacity"; to: 0; duration: AppMotion.scaled(300) }
        }
    }

    contentItem: SettingsPage {
        id: settingsPage
        // Header (~64) + divider + footer (~76) around the page stack.
        pageHeight: Math.max(320, root.height - 150)
        closeSheet: function () { root.close() }
    }
}
