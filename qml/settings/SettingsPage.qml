import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var closeSheet

    spacing: 0

    readonly property int contentMargin: MD.Token.spacing.large

    property bool applying: false

    function syncFromStore() {
        applying = true
        Appearance.apply()
        paletteListView.currentIndex = Appearance.paletteType
        libraryPathField.text = Core.settings.libraryRoot
        downloadsPathField.text = Core.settings.downloadsRoot
        catalogUrlField.text = Core.settings.freetpCatalogUrl
        applying = false
    }

    MD.DialogHeader {
        Layout.fillWidth: true
        title: qsTr("Настройки")
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: contentMargin
        Layout.rightMargin: contentMargin
        Layout.topMargin: MD.Token.spacing.small
        spacing: MD.Token.spacing.large

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Хранилище и загрузки")
            typescale: MD.Token.typescale.title_medium
        }

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Папка библиотеки — куда плагины будут устанавливать игры. Папка загрузок — торренты до установки.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        MD.TextField {
            id: libraryPathField
            Layout.fillWidth: true
            placeholderText: qsTr("Папка библиотеки")
            onEditingFinished: {
                if (!root.applying)
                    Core.settings.libraryRoot = text
            }
        }

        MD.TextField {
            id: downloadsPathField
            Layout.fillWidth: true
            placeholderText: qsTr("Папка загрузок")
            onEditingFinished: {
                if (!root.applying)
                    Core.settings.downloadsRoot = text
            }
        }

        MD.TextField {
            id: catalogUrlField
            Layout.fillWidth: true
            placeholderText: qsTr("Каталог FreeTP (JSON)")
            onEditingFinished: {
                if (!root.applying)
                    Core.settings.freetpCatalogUrl = text
            }
        }

        MD.AutoDivider { Layout.fillWidth: true }

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Внешний вид")
            typescale: MD.Token.typescale.title_medium
        }

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Тема и палитра Material 3 применяются ко всему приложению.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.medium

            MD.Label {
                text: MD.Token.isDarkTheme ? qsTr("Тёмная тема") : qsTr("Светлая тема")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_large
            }

            MD.Switch {
                checked: MD.Token.isDarkTheme
                onCheckedChanged: {
                    if (root.applying)
                        return
                    Appearance.setThemeMode(checked ? MD.Enum.Dark : MD.Enum.Light)
                }
            }

            Item { Layout.fillWidth: true }
        }

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Палитра")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }

        MD.HorizontalListView {
            id: paletteListView
            Layout.fillWidth: true
            expand: true
            spacing: MD.Token.spacing.small
            implicitHeight: 40
            model: MD.PaletteModel {}

            MD.ActionGroup {
                id: paletteActionGroup
            }

            delegate: MD.InputChip {
                required property int index
                required property var model

                action: MD.Action {
                    T.ActionGroup.group: paletteActionGroup
                    icon.name: ""
                    checkable: true
                    checked: paletteListView.currentIndex === index
                    text: model.name
                    onTriggered: {
                        if (root.applying)
                            return
                        paletteListView.currentIndex = index
                        Appearance.setPaletteType(index)
                    }
                }
            }
        }

        MD.AutoDivider {
            Layout.fillWidth: true
        }

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Primary")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }

        Grid {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: MD.Token.spacing.small
            spacing: MD.Token.spacing.medium
            rows: 2
            columns: 5

            Repeater {
                model: AccentColors.palette

                MD.ColorRadio {
                    required property var modelData
                    size: 32
                    color: modelData.color
                    checked: Appearance.accentColor === modelData.color
                    onClicked: {
                        if (root.applying)
                            return
                        Appearance.setAccentColor(modelData.color)
                    }
                }
            }
        }
    }

    MD.Divider {
        Layout.fillWidth: true
        Layout.topMargin: MD.Token.spacing.medium
        Layout.leftMargin: contentMargin
        Layout.rightMargin: contentMargin
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignRight
        Layout.rightMargin: contentMargin
        Layout.bottomMargin: MD.Token.spacing.large
        Layout.topMargin: MD.Token.spacing.small

        MD.Button {
            mdState.type: MD.Enum.BtText
            text: qsTr("Готово")
            onClicked: root.closeSheet()
        }
    }
}
