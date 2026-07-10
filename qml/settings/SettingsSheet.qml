import QtQuick

import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal

    onAboutToShow: settingsPage.syncFromStore()
    onClosed: settingsPage.resetOnClose()

    function openSettings() {
        settingsPage.prepareOpen("", false)
        open()
    }

    function openSources(createSource) {
        settingsPage.prepareOpen("sources", !!createSource)
        open()
    }

    SettingsPage {
        id: settingsPage
        width: root.sheetWidth
        closeSheet: function () { root.close() }
    }
}
