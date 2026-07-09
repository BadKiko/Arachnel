import QtQuick

import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal

    onAboutToShow: settingsPage.syncFromStore()

    SettingsPage {
        id: settingsPage
        width: root.sheetWidth
        closeSheet: root.close
    }
}
