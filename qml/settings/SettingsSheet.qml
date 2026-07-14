import QtQuick

import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal
    // Wheel over the sheet must scroll content, not pull the modal down (Flickable2 dismiss).
    dismissOnDragDown: false

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

    function openLaunch() {
        if (Qt.platform.os !== "linux")
            return
        settingsPage.prepareOpen("launch", false)
        open()
    }

    SettingsPage {
        id: settingsPage
        width: root.sheetWidth
        closeSheet: function () { root.close() }
    }
}
