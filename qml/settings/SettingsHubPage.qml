import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

ColumnLayout {
    id: root

    property int contentMargin: MD.Token.spacing.large

    signal openSection(string sectionId)

    spacing: MD.Token.spacing.small

    MD.Label {
        Layout.fillWidth: true
        Layout.leftMargin: contentMargin
        Layout.rightMargin: contentMargin
        Layout.topMargin: MD.Token.spacing.small
        text: qsTr("Выберите раздел — каждый экран отвечает за свою часть настроек.")
        wrapMode: Text.WordWrap
        color: MD.Token.color.on_surface_variant
        typescale: MD.Token.typescale.body_medium
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: contentMargin
        Layout.rightMargin: contentMargin
        Layout.topMargin: MD.Token.spacing.small
        spacing: MD.Token.spacing.small

        Repeater {
            model: [
                {
                    id: "sources",
                    title: qsTr("Источники"),
                    subtitle: qsTr("Каталоги JSON, включение и URL")
                },
                {
                    id: "storage",
                    title: qsTr("Хранилище"),
                    subtitle: qsTr("Папки библиотеки и загрузок")
                },
                {
                    id: "appearance",
                    title: qsTr("Внешний вид"),
                    subtitle: qsTr("Тема, палитра и акцентный цвет")
                }
            ]

            Rectangle {
                id: sectionCard
                required property var modelData

                Layout.fillWidth: true
                radius: MD.Token.shape.corner.large
                color: sectionMouse.containsMouse
                       ? MD.Token.color.surface_container_high
                       : MD.Token.color.surface_container
                border.width: 1
                border.color: MD.Token.color.outline_variant
                implicitHeight: sectionRow.implicitHeight + MD.Token.spacing.medium * 2

                Behavior on color {
                    ColorAnimation { duration: MD.Token.duration.short4 }
                }

                RowLayout {
                    id: sectionRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: MD.Token.spacing.medium
                    anchors.rightMargin: MD.Token.spacing.medium
                    spacing: MD.Token.spacing.medium

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        MD.Label {
                            Layout.fillWidth: true
                            text: sectionCard.modelData.title
                            typescale: MD.Token.typescale.title_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: sectionCard.modelData.subtitle
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_small
                            elide: Text.ElideRight
                        }
                    }

                    MD.Icon {
                        name: MD.Token.icon.chevron_right
                        size: 24
                        color: MD.Token.color.on_surface_variant
                    }
                }

                MouseArea {
                    id: sectionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.openSection(sectionCard.modelData.id)
                }
            }
        }
    }
}
