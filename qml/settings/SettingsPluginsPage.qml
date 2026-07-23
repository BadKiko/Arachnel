import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property var pluginRows: []

    signal openStoreRequested

    function reloadPlugins() {
        pluginRows = Core.pluginEntries()
    }

    Component.onCompleted: reloadPlugins()

    Connections {
        target: Core
        function onPluginsChanged() {
            root.reloadPlugins()
        }
    }

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
            text: Messages.settingsPluginsDesc
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        MD.Button {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            mdState.type: MD.Enum.BtFilled
            text: qsTr("Plugin store")
            icon.name: MD.Token.icon.storefront
            onClicked: root.openStoreRequested()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.pluginRows.length === 0
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: emptyCol.implicitHeight + MD.Token.spacing.medium * 2

            ColumnLayout {
                id: emptyCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.extra_small

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("No plugins installed")
                    typescale: MD.Token.typescale.title_small
                }

                MD.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Open the plugin store or install a plugin file you already have.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.small
            visible: root.pluginRows.length > 0

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Installed plugins")
                typescale: MD.Token.typescale.title_small
            }

            Repeater {
                model: root.pluginRows

                Rectangle {
                    required property var modelData

                    Layout.fillWidth: true
                    radius: MD.Token.shape.corner.large
                    color: MD.Token.color.surface_container
                    border.width: 1
                    border.color: MD.Token.color.outline_variant
                    implicitHeight: cardCol.implicitHeight + MD.Token.spacing.medium * 2

                    ColumnLayout {
                        id: cardCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: MD.Token.spacing.medium
                        spacing: MD.Token.spacing.extra_small

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    typescale: MD.Token.typescale.title_small
                                    elide: Text.ElideRight
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: modelData.description
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_small
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                            }

                            MD.Button {
                                mdState.type: MD.Enum.BtText
                                text: qsTr("Delete")
                                icon.name: MD.Token.icon.delete
                                onClicked: {
                                    removeDialog.pluginId = modelData.pluginId
                                    removeDialog.pluginName = modelData.name
                                    removeDialog.open()
                                }
                            }
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: modelData.loaded === false
                                  ? qsTr("v%1 · %2 — not loaded").arg(modelData.pluginVersion).arg(modelData.pluginId)
                                  : qsTr("v%1 · %2").arg(modelData.pluginVersion).arg(modelData.pluginId)
                            color: modelData.loaded === false ? MD.Token.color.error : MD.Token.color.primary
                            typescale: MD.Token.typescale.label_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: modelData.pluginRootPath
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.label_small
                            elide: Text.ElideMiddle
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: Core.lastPluginError.length > 0
            text: Core.lastPluginError
            color: MD.Token.color.error
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_small
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtFilledTonal
                text: qsTr("Install from file…")
                onClicked: Core.browsePluginArach()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.small

                MD.Button {
                    Layout.fillWidth: true
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Open folder")
                    onClicked: Core.openPluginsFolder()
                }

                MD.Button {
                    Layout.fillWidth: true
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Refresh")
                    onClicked: {
                        Core.rescanPlugins()
                        root.reloadPlugins()
                    }
                }
            }

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("User-installed: %1").arg(Core.pluginsUserDir)
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_small
                wrapMode: Text.WordWrap
            }
        }
    }

    MD.Dialog {
        id: removeDialog
        parent: Overlay.overlay
        modal: true
        property string pluginId: ""
        property string pluginName: ""
        title: qsTr("Remove plugin?")

        contentItem: ColumnLayout {
            spacing: MD.Token.spacing.medium
            width: parent ? parent.width : implicitWidth

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Remove \"%1\"? Catalogs from this plugin will stop working until you install it again.")
                      .arg(removeDialog.pluginName)
                wrapMode: Text.WordWrap
                typescale: MD.Token.typescale.body_medium
            }
        }

        footer: Item {
            implicitHeight: footerRow.implicitHeight + MD.Token.spacing.medium

            MD.DialogButtonBox {
                id: footerRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top

                MD.Button {
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Cancel")
                    DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                    onClicked: removeDialog.close()
                }

                MD.Button {
                    mdState.type: MD.Enum.BtFilled
                    text: qsTr("Delete")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    onClicked: {
                        const id = removeDialog.pluginId
                        removeDialog.close()
                        Core.uninstallPlugin(id)
                    }
                }
            }
        }
    }

}
