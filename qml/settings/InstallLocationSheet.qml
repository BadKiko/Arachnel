import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal
    property string entryId: ""
    property string entryTitle: ""
    property string selectedLibraryId: Core.settings.storageLibraries.defaultLibraryId

    function openForEntry(id, title) {
        entryId = id
        entryTitle = title || ""
        selectedLibraryId = Core.settings.storageLibraries.defaultLibraryId
        open()
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: qsTr("Установить")
            typescale: MD.Token.typescale.headline_medium
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: entryTitle.length ? entryTitle : qsTr("Выберите диск для установки")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: qsTr("Установить на:")
            typescale: MD.Token.typescale.label_large
            color: MD.Token.color.primary
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small

            Repeater {
                model: Core.settings.storageLibraries

                Rectangle {
                    required property string libraryId
                    required property string label
                    required property string path
                    required property bool isDefault

                    Layout.fillWidth: true
                    radius: MD.Token.shape.corner.large
                    color: root.selectedLibraryId === libraryId
                           ? MD.Token.color.secondary_container
                           : MD.Token.color.surface_container
                    border.width: 1
                    border.color: root.selectedLibraryId === libraryId
                                  ? MD.Token.color.primary
                                  : MD.Token.color.outline_variant
                    implicitHeight: row.implicitHeight + MD.Token.spacing.medium * 2

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.selectedLibraryId = libraryId
                    }

                    RowLayout {
                        id: row
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: MD.Token.spacing.medium
                        spacing: MD.Token.spacing.small

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.extra_small

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: label
                                    typescale: MD.Token.typescale.title_small
                                }

                                MD.Icon {
                                    visible: isDefault
                                    name: MD.Token.icon.star
                                    size: 18
                                    color: MD.Token.color.primary
                                }
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: path
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.body_small
                                elide: Text.ElideMiddle
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtText
                text: qsTr("Отмена")
                onClicked: root.close()
            }

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtFilled
                text: qsTr("Установить")
                enabled: root.entryId.length > 0 && root.selectedLibraryId.length > 0
                onClicked: {
                    Core.installCatalogEntry(root.entryId, root.selectedLibraryId)
                    root.close()
                }
            }
        }
    }
}
