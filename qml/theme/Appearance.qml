pragma Singleton

import QtQuick
import QtCore
import Qcm.Material as MD

Item {
    id: root

    property bool applying: false

    readonly property int defaultThemeMode: MD.Enum.Dark
    // Launcher v2 starts colourful: the Expressive palette from a violet seed. It keeps its own
    // settings group, so the classic look's choices are untouched.
    readonly property int defaultPaletteType: MD.Enum.PaletteExpressive
    readonly property string defaultAccentColor: "#7C5CFF"

    Settings {
        id: store
        category: "appearance_v2"
        property int themeMode: root.defaultThemeMode
        property int paletteType: root.defaultPaletteType
        property string accentColor: root.defaultAccentColor
        property bool reduceMotion: false
        // Muting one trailer mutes the next ones too, until the user turns the sound back on.
        property bool trailerMuted: false
    }

    readonly property int themeMode: store.themeMode
    readonly property int paletteType: store.paletteType
    readonly property string accentColor: store.accentColor
    readonly property bool reduceMotion: store.reduceMotion
    readonly property bool trailerMuted: store.trailerMuted

    // One page gutter for every top-level page.
    readonly property int pageMargin: MD.Token.spacing.large

    function apply() {

        MD.Token.color.useSysColorSM = false
        MD.Token.color.useSysAccentColor = false
        MD.Token.themeMode = store.themeMode
        MD.Token.color.paletteType = store.paletteType
        MD.Token.color.accentColor = store.accentColor
        Qt.styleHints.colorScheme = store.themeMode === MD.Enum.Dark ? Qt.Dark : Qt.Light
    }

    function setTrailerMuted(muted) {
        store.trailerMuted = muted
    }

    function setReduceMotion(enabled) {
        store.reduceMotion = enabled
    }

    function setThemeMode(mode) {
        store.themeMode = mode
        if (!applying) {
            MD.Token.themeMode = mode
            Qt.styleHints.colorScheme = mode === MD.Enum.Dark ? Qt.Dark : Qt.Light
        }
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
