import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Qcm.Material as MD

// Colour choice the way Android does it: every option is a round swatch that shows the tones the
// app will actually use (primary, secondary, tertiary and the surface), so you pick a look, not a
// hex value. Below, the colour style as named cards with a small preview of the same three tones.
ColumnLayout {
    id: root

    property color seed: Appearance.accentColor
    property int paletteType: Appearance.paletteType
    property bool applying: false

    readonly property bool dark: MD.Token.isDarkTheme
    readonly property int accentTone: dark ? 80 : 40
    readonly property int surfaceTone: dark ? 30 : 90

    spacing: MD.Token.spacing.medium

    // Approximate the three key tones a style derives from a seed (HCT hue / chroma), enough for a
    // faithful preview without building the full scheme.
    function tones(seedColor, type) {
        const h = MD.HctUtil.hueOf(seedColor)
        const c = MD.HctUtil.chromaOf(seedColor)
        function hc(hue, chroma) { return MD.HctUtil.fromHct((hue + 360) % 360, chroma, root.accentTone) }
        switch (type) {
        case MD.Enum.PaletteMonochrome:
            return [MD.HctUtil.fromHct(h, 0, root.accentTone), MD.HctUtil.fromHct(h, 0, root.accentTone - 20),
                    MD.HctUtil.fromHct(h, 0, root.accentTone - 35)]
        case MD.Enum.PaletteNeutral:
            return [hc(h, 12), hc(h, 8), hc(h + 60, 16)]
        case MD.Enum.PaletteVibrant:
            return [hc(h, 200), hc(h + 15, 24), hc(h + 60, 32)]
        case MD.Enum.PaletteExpressive:
            return [hc(h + 240, 40), hc(h + 300, 24), hc(h + 30, 32)]
        case MD.Enum.PaletteRainbow:
            return [hc(h, 48), hc(h, 16), hc(h + 60, 24)]
        case MD.Enum.PaletteFruitSalad:
            return [hc(h - 50, 48), hc(h - 50, 36), hc(h, 36)]
        case MD.Enum.PaletteContent:
        case MD.Enum.PaletteFidelity:
            return [hc(h, Math.max(c, 36)), hc(h, Math.max(c - 32, c * 0.5)), hc(h + 60, Math.max(16, c * 0.6))]
        }
        return [hc(h, 36), hc(h, 16), hc(h + 60, 24)] // TonalSpot
    }

    readonly property var styles: [
        { type: MD.Enum.PaletteTonalSpot, name: qsTr("Calm"), hint: qsTr("Soft, balanced tones") },
        { type: MD.Enum.PaletteVibrant, name: qsTr("Vibrant"), hint: qsTr("Rich, saturated colour") },
        { type: MD.Enum.PaletteExpressive, name: qsTr("Expressive"), hint: qsTr("Playful contrasting accents") },
        { type: MD.Enum.PaletteRainbow, name: qsTr("Rainbow"), hint: qsTr("Bright accents, neutral surfaces") },
        { type: MD.Enum.PaletteFruitSalad, name: qsTr("Fruit salad"), hint: qsTr("Warm shifted hues") },
        { type: MD.Enum.PaletteNeutral, name: qsTr("Muted"), hint: qsTr("Barely tinted greys") },
        { type: MD.Enum.PaletteMonochrome, name: qsTr("Monochrome"), hint: qsTr("Black, white and grey") },
        { type: MD.Enum.PaletteFidelity, name: qsTr("True colour"), hint: qsTr("Keeps your colour as is") }
    ]

    // ---------- Swatches ----------
    Flow {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.medium

        Repeater {
            model: AccentColors.palette

            delegate: Item {
                id: swatch
                required property var modelData
                readonly property bool chosen: String(root.seed).toLowerCase() === String(modelData.color).toLowerCase()
                readonly property var t: root.tones(modelData.color, root.paletteType)

                width: 64
                height: 64

                // Selection ring.
                Rectangle {
                    anchors.fill: parent
                    radius: swatch.chosen ? 20 : 32
                    color: "transparent"
                    border.width: swatch.chosen ? 3 : 0
                    border.color: MD.Token.color.primary
                    Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
                }

                // Four tones in one circle: primary on top, secondary and tertiary below, surface in
                // the middle - like the colour options in Android's wallpaper and style settings.
                Item {
                    id: disc
                    anchors.centerIn: parent
                    width: 52
                    height: 52
                    scale: tap.pressed ? 0.9 : (hover.hovered ? 1.06 : 1)
                    Behavior on scale { NumberAnimation { duration: AppMotion.short; easing.type: Easing.OutBack } }
                    layer.enabled: true
                    layer.effect: MD.RoundClip {
                        corners: MD.Util.corners(26)
                        size: Qt.vector2d(disc.width, disc.height)
                    }

                    Rectangle { x: 0; y: 0; width: 52; height: 26; color: swatch.t[0] }
                    Rectangle { x: 0; y: 26; width: 26; height: 26; color: swatch.t[1] }
                    Rectangle { x: 26; y: 26; width: 26; height: 26; color: swatch.t[2] }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 22
                        height: 22
                        radius: 11
                        color: MD.HctUtil.paletteTone(swatch.modelData.color, root.surfaceTone)

                        MD.Icon {
                            anchors.centerIn: parent
                            visible: swatch.chosen
                            name: MD.Token.icon.check
                            size: 16
                            color: MD.HctUtil.paletteTone(swatch.modelData.color, root.dark ? 90 : 10)
                        }
                    }
                }

                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    id: tap
                    onTapped: if (!root.applying) Appearance.setAccentColor(swatch.modelData.color)
                }
                MD.ToolTip {
                    visible: hover.hovered
                    text: swatch.modelData.name
                }
            }
        }

        // Any colour you like, from the wheel.
        Item {
            width: 64
            height: 64

            Rectangle {
                anchors.centerIn: parent
                width: 52
                height: 52
                radius: 26
                color: MD.Token.color.surface_container_highest
                border.width: 2
                border.color: MD.Token.color.outline_variant
                scale: customTap.pressed ? 0.9 : (customHover.hovered ? 1.06 : 1)
                Behavior on scale { NumberAnimation { duration: AppMotion.short; easing.type: Easing.OutBack } }

                MD.Icon {
                    anchors.centerIn: parent
                    name: MD.Token.icon.colorize
                    size: 22
                    color: MD.Token.color.on_surface_variant
                }
            }

            HoverHandler { id: customHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { id: customTap; onTapped: customDialog.open() }
            MD.ToolTip {
                visible: customHover.hovered
                text: qsTr("Custom colour")
            }

            MD.ColorPickerDialog {
                id: customDialog
                parent: Overlay.overlay
                color: root.seed
                showAlpha: false
                onAcceptedColor: function (c) {
                    if (!root.applying)
                        Appearance.setAccentColor(c.toString())
                }
            }
        }
    }

    // ---------- Style ----------
    MD.Label {
        text: qsTr("Style")
        typescale: MD.Token.typescale.title_small
        color: MD.Token.color.on_surface_variant
    }

    GridLayout {
        Layout.fillWidth: true
        columns: width > 760 ? 4 : (width > 440 ? 3 : 2)
        columnSpacing: MD.Token.spacing.small
        rowSpacing: MD.Token.spacing.small

        Repeater {
            model: root.styles

            delegate: Rectangle {
                id: styleCard
                required property var modelData
                readonly property bool chosen: root.paletteType === modelData.type
                readonly property var t: root.tones(root.seed, modelData.type)

                Layout.fillWidth: true
                Layout.fillHeight: true
                implicitHeight: styleCol.implicitHeight + 2 * MD.Token.spacing.medium
                radius: chosen ? 16 : 24
                color: chosen ? MD.Token.color.secondary_container
                     : (styleHover.hovered ? MD.Token.color.surface_container_highest : MD.Token.color.surface_container_high)
                border.width: chosen ? 2 : 0
                border.color: MD.Token.color.primary
                Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
                Behavior on color { ColorAnimation { duration: AppMotion.short } }

                ColumnLayout {
                    id: styleCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.medium
                    spacing: 4

                    Row {
                        spacing: -6
                        Repeater {
                            model: 3
                            Rectangle {
                                required property int index
                                width: 22
                                height: 22
                                radius: 11
                                color: styleCard.t[index]
                                border.width: 2
                                border.color: styleCard.color
                            }
                        }
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: styleCard.modelData.name
                        typescale: MD.Token.typescale.title_small
                        color: styleCard.chosen ? MD.Token.color.on_secondary_container : MD.Token.color.on_surface
                        elide: Text.ElideRight
                    }
                    MD.Label {
                        Layout.fillWidth: true
                        text: styleCard.modelData.hint
                        typescale: MD.Token.typescale.label_small
                        color: MD.Token.color.on_surface_variant
                        wrapMode: Text.WordWrap
                    }
                }

                HoverHandler { id: styleHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: if (!root.applying) Appearance.setPaletteType(styleCard.modelData.type)
                }
            }
        }
    }
}
