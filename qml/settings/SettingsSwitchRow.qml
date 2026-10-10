import QtQuick

import Qcm.Material as MD

// A settings row with a switch; tapping anywhere on the row flips it.
SettingsRow {
    id: root

    property bool checked: false
    signal toggled(bool checked)

    clickable: true
    onClicked: root.toggled(!root.checked)

    MD.Switch {
        checked: root.checked
        onToggled: root.toggled(checked)
    }
}
