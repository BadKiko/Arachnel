pragma Singleton

import QtQuick
import Qcm.Material as MD

QtObject {
    function apply() {
        MD.Token.color.useSysColorSM = false
        MD.Token.color.useSysAccentColor = false
        MD.Token.themeMode = MD.Enum.Dark
        MD.Token.color.paletteType = MD.Enum.PaletteMonochrome
        MD.Token.color.accentColor = "#8E8E93"
        Qt.styleHints.colorScheme = Qt.Dark
    }
}
