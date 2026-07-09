pragma Singleton

import QtQuick
import QtCore
import Qcm.Material as MD

Item {
    id: root

    property bool applying: false

    readonly property int defaultThemeMode: MD.Enum.Dark
    readonly property int defaultPaletteType: 2
    readonly property string defaultAccentColor: "#984300"

    Settings {
        id: store
        category: "appearance"
        property int themeMode: root.defaultThemeMode
        property int paletteType: root.defaultPaletteType
        property string accentColor: root.defaultAccentColor
    }

    readonly property int themeMode: store.themeMode
    readonly property int paletteType: store.paletteType
    readonly property string accentColor: store.accentColor

    function apply() {
        MD.Token.color.useSysColorSM = false
        MD.Token.color.useSysAccentColor = false
        MD.Token.themeMode = store.themeMode
        MD.Token.color.paletteType = store.paletteType
        MD.Token.color.accentColor = store.accentColor
    }

    function setThemeMode(mode) {
        store.themeMode = mode
        if (!applying)
            MD.Token.themeMode = mode
    }

    function setPaletteType(paletteType) {
        store.paletteType = paletteType
        if (!applying)
            MD.Token.color.paletteType = paletteType
    }

    function setAccentColor(color) {
        const hex = typeof color === "string" ? color : color.toString()
        store.accentColor = hex
        if (!applying) {
            MD.Token.color.useSysAccentColor = false
            MD.Token.color.accentColor = hex
        }
    }
}
