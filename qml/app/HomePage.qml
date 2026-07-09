import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

Item {
    id: root

    readonly property string tagline: qsTr(
        "Лаунчер игр с плагинами источников — online-fix, FreeTP и др.")

    readonly property string pitch: qsTr(
        "Один интерфейс библиотеки и запуска. У каждого источника свой плагин: "
        + "portable, установщик, встроенный или отдельный фикс.")

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * MD.Token.spacing.large, 560)
        spacing: MD.Token.spacing.large

        MD.Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Arachnel")
            typescale: MD.Token.typescale.headline_medium
            color: MD.Token.color.primary
        }

        MD.Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: root.tagline
            typescale: MD.Token.typescale.title_small
            color: MD.Token.color.on_surface
        }

        MD.Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: root.pitch
            typescale: MD.Token.typescale.body_medium
            color: MD.Token.color.on_surface_variant
        }

        MD.Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Скелет UI · Core и плагины — в разработке")
            typescale: MD.Token.typescale.label_medium
            color: MD.Token.color.outline
        }
    }
}
