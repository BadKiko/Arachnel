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

    readonly property var steamPlugin: {
        const rows = root.pluginRows || []
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].pluginId === "steamidra")
                return rows[i]
        }
        return null
    }

    readonly property var extraPlugins: {
        const rows = (root.pluginRows || []).slice()
        const out = []
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].pluginId !== "steamidra")
                out.push(rows[i])
        }
        out.sort(function (a, b) {
            return (a.name || "").localeCompare(b.name || "")
        })
        return out
    }

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

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: !root.steamPlugin && root.extraPlugins.length === 0
            implicitHeight: emptyCol.implicitHeight + MD.Token.spacing.large * 2
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: emptyCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: MD.Token.spacing.large
                spacing: MD.Token.spacing.small

                MD.ElevationRectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: MD.Token.shape.corner.full
                    color: MD.Token.color.primary_container
                    elevation: MD.Token.elevation.level0

                    MD.Icon {
                        anchors.centerIn: parent
                        name: MD.Token.icon.stadia_controller
                        size: 24
                        color: MD.Token.color.on_primary_container
                    }
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("No plugins installed")
                    typescale: MD.Token.typescale.title_small
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Open the plugin store. Steam is the one to get first.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }
            }
        }

        OfficialPluginCard {
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: !!root.steamPlugin
            plugin: root.steamPlugin || ({})
            featured: true
            installed: true
            loaded: root.steamPlugin ? root.steamPlugin.loaded !== false : true
            showSource: false
            onUninstallClicked: {
                removeDialog.pluginId = "steamidra"
                removeDialog.pluginName = qsTr("Steam")
                removeDialog.open()
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.extraPlugins.length > 0
            text: qsTr("Other plugins")
            typescale: MD.Token.typescale.title_small
        }

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.extraPlugins.length > 0
            implicitHeight: extraCol.implicitHeight + MD.Token.spacing.small * 2
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: extraCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.small
                spacing: 0

                Repeater {
                    model: root.extraPlugins

                    ColumnLayout {
                        id: extraRow
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        spacing: 0

                        readonly property string versionLabel: {
                            const v = modelData.pluginVersion || ""
                            return v.length ? qsTr("v%1").arg(v) : ""
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: MD.Token.spacing.small
                            Layout.rightMargin: MD.Token.spacing.extra_small
                            Layout.topMargin: MD.Token.spacing.small
                            Layout.bottomMargin: MD.Token.spacing.small
                            spacing: MD.Token.spacing.medium

                            MD.ElevationRectangle {
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                Layout.alignment: Qt.AlignVCenter
                                radius: MD.Token.shape.corner.full
                                color: MD.Token.color.surface_container_highest
                                elevation: MD.Token.elevation.level0

                                MD.Icon {
                                    anchors.centerIn: parent
                                    name: MD.Token.icon.extension
                                    size: 20
                                    color: MD.Token.color.on_surface_variant
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.extra_small

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: extraRow.modelData.name
                                    typescale: MD.Token.typescale.body_large
                                    elide: Text.ElideRight
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    visible: extraRow.modelData.loaded === false
                                             || extraRow.versionLabel.length > 0
                                    text: extraRow.modelData.loaded === false
                                          ? qsTr("Not loaded")
                                          : extraRow.versionLabel
                                    color: extraRow.modelData.loaded === false
                                           ? MD.Token.color.error
                                           : MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_small
                                }
                            }

                            MD.IconButton {
                                mdState.type: MD.Enum.IBtStandard
                                icon.name: MD.Token.icon.delete
                                onClicked: {
                                    removeDialog.pluginId = extraRow.modelData.pluginId
                                    removeDialog.pluginName = extraRow.modelData.name
                                    removeDialog.open()
                                }
                            }
                        }

                        MD.Divider {
                            Layout.fillWidth: true
                            visible: extraRow.index < root.extraPlugins.length - 1
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

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtText
                text: qsTr("Install from file…")
                onClicked: Core.browsePluginArach()
            }

            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("Open folder")
                onClicked: Core.openPluginsFolder()
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
