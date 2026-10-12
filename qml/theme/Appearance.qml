pragma Singleton

import QtQuick
import QtCore
import Qcm.Material as MD

Item {
    id: root

    property bool applying: false

    readonly property int defaultThemeMode: MD.Enum.Dark
    // Launcher v2 is monochrome (black, white and grey) by default. It keeps its own settings
    // group, so the classic look's choices are untouched.
    readonly property int defaultPaletteType: MD.Enum.PaletteMonochrome
    readonly property string defaultAccentColor: "#7C5CFF"

    Settings {
        id: store
        category: "appearance_v2"
        property int themeMode: root.defaultThemeMode
        property int paletteType: root.defaultPaletteType
        property string accentColor: root.defaultAccentColor
        // Bumped when a default changes for everyone (see migrateDefaults).
        property int defaultsVersion: 0
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

    readonly property int currentDefaultsVersion: 1

    // Settings stores the defaults on first run, so changing a default only reaches new installs.
    // Version 1 made the palette monochrome for everyone: the stored palette is replaced once, and
    // whatever the user picks afterwards is kept.
    function migrateDefaults() {
        if (store.defaultsVersion >= root.currentDefaultsVersion)
            return
        store.paletteType = root.defaultPaletteType
        store.defaultsVersion = root.currentDefaultsVersion
    }

    function apply() {
        migrateDefaults()
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
