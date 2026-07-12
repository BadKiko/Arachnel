import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Templates as T

import Qcm.Material as MD

MD.Dialog {
    id: root

    parent: Overlay.overlay
    title: qsTr("Что такое источник?")
    standardButtons: T.DialogButtonBox.Close
    modal: true

    readonly property var steps: [
        {
            icon: MD.Token.icon.extension,
            step: qsTr("Шаг 1"),
            title: qsTr("Источник"),
            body: qsTr("Это JSON-каталог игр по URL — формат Hydra/FreeTP. Укажите ссылку в настройках, Arachnel загрузит список.")
        },
        {
            icon: MD.Token.icon.storefront,
            step: qsTr("Шаг 2"),
            title: qsTr("Каталог"),
            body: qsTr("Игры из включённых источников появляются в «Каталоге». У каждого источника свой способ установки.")
        },
        {
            icon: MD.Token.icon.sports_esports,
            step: qsTr("Шаг 3"),
            title: qsTr("Библиотека"),
            body: qsTr("После загрузки и установки игра окажется здесь — запуск, обновления и детали.")
        }
    ]

    contentItem: ColumnLayout {
        spacing: MD.Token.spacing.medium
        width: parent ? parent.width : implicitWidth

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Источник — не сайт магазина, а подключаемый каталог. Сейчас это JSON-фид; позже — плагины с разными пайплайнами установки.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        Repeater {
            model: root.steps

            Rectangle {
                required property var modelData

                Layout.fillWidth: true
                radius: MD.Token.shape.corner.large
                color: MD.Token.color.surface_container
                implicitHeight: stepCol.implicitHeight + MD.Token.spacing.medium * 2

                ColumnLayout {
                    id: stepCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.medium
                    spacing: MD.Token.spacing.small

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.medium

                        Rectangle {
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 40
                            radius: MD.Token.shape.corner.medium
                            color: MD.Token.color.secondary_container

                            MD.Icon {
                                anchors.centerIn: parent
                                name: modelData.icon
                                size: 22
                                color: MD.Token.color.on_secondary_container
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            MD.Label {
                                Layout.fillWidth: true
                                text: modelData.step
                                color: MD.Token.color.primary
                                typescale: MD.Token.typescale.label_medium
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: modelData.title
                                typescale: MD.Token.typescale.title_small
                            }
                        }
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: modelData.body
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
