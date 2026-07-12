import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: qsTr("Проверка обновлений и целостности portable-сборок.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.medium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Проверять обновления при загрузке каталога")
                    typescale: MD.Token.typescale.body_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Сравнивает даты сборок в каталоге с установленными играми.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                    wrapMode: Text.WordWrap
                }
            }

            MD.Switch {
                checked: Core.settings.autoCheckUpdates
                onToggled: Core.settings.autoCheckUpdates = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.medium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Проверять файлы portable перед запуском")
                    typescale: MD.Token.typescale.body_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Папка установки и .exe должны существовать.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                    wrapMode: Text.WordWrap
                }
            }

            MD.Switch {
                checked: Core.settings.verifyPortableFiles
                onToggled: Core.settings.verifyPortableFiles = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.small

            MD.Button {
                text: qsTr("Проверить обновления")
                icon.name: MD.Token.icon.update
                mdState.type: MD.Enum.BtOutlined
                onClicked: Core.checkUpdates()
            }

            MD.Button {
                text: qsTr("Проверить portable")
                icon.name: MD.Token.icon.fact_check
                mdState.type: MD.Enum.BtOutlined
                onClicked: Core.verifyAllPortableGames()
            }
        }
    }
}
