import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property var pluginRows: []

    readonly property var catalogPlugins: Core.pluginCatalog ? Core.pluginCatalog.plugins : []
    readonly property bool catalogBusy: !!(Core.pluginCatalog && Core.pluginCatalog.installing)

    function reloadPlugins() {
        pluginRows = Core.pluginEntries()
    }

    function isPluginInstalled(pluginId) {
        for (let i = 0; i < pluginRows.length; ++i) {
            if (pluginRows[i].pluginId === pluginId)
                return true
        }
        return false
    }

    function thisInstalling(pluginId) {
        return Core.pluginCatalog && Core.pluginCatalog.installing
                && Core.pluginCatalog.installingPluginId === pluginId
    }

    function requestUninstall(pluginId, pluginName) {
        removeDialog.pluginId = pluginId
        removeDialog.pluginName = pluginName
        removeDialog.open()
    }

    Component.onCompleted: {
        reloadPlugins()
        Core.refreshOfficialPlugins()
    }

    Connections {
        target: Core
        function onPluginsChanged() {
            root.reloadPlugins()
        }
    }

    contentWidth: width
    contentHeight: body.implicitHeight + MD.Token.spacing.extra_large
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    SettingsColumn {
        id: body
        margin: root.contentMargin

        SettingsGroup {
            title: qsTr("Official plugins")
            description: Messages.settingsPluginsDesc

            SettingsRow {
                visible: (Core.pluginCatalog && Core.pluginCatalog.loading)
                         || (Core.pluginCatalog && Core.pluginCatalog.error.length > 0)
                         || root.catalogPlugins.length === 0
                title: Core.pluginCatalog && Core.pluginCatalog.loading
                       ? qsTr("Loading official plugins…")
                       : (Core.pluginCatalog && Core.pluginCatalog.error.length > 0
                          ? Core.pluginCatalog.error
                          : qsTr("No official plugins available for this platform."))
                titleColor: Core.pluginCatalog && Core.pluginCatalog.error.length > 0
                            ? MD.Token.color.error : MD.Token.color.on_surface
                iconName: MD.Token.icon.storefront

                MD.Button {
                    mdState.type: MD.Enum.BtText
                    text: qsTr("Refresh list")
                    enabled: !(Core.pluginCatalog && Core.pluginCatalog.loading)
                    onClicked: Core.refreshOfficialPlugins()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                visible: root.catalogPlugins.length > 0
                implicitHeight: storeCol.implicitHeight + MD.Token.spacing.small * 2
                radius: 24
                color: MD.Token.color.surface_container

                ColumnLayout {
                    id: storeCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.small
                    spacing: 0

                    Repeater {
                        model: root.catalogPlugins

                        OfficialPluginCard {
                            required property var modelData
                            required property int index
                            plugin: modelData
                            installed: root.isPluginInstalled(modelData.id)
                            installing: root.thisInstalling(modelData.id)
                            catalogBusy: root.catalogBusy
                            showDivider: index < root.catalogPlugins.length - 1
                            onInstallClicked: Core.installOfficialPlugin(modelData.id)
                            onUninstallClicked: root.requestUninstall(modelData.id, modelData.name)
                            onSourceClicked: function (url) { Core.openExternalUrl(url) }
                        }
                    }
                }
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
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: MD.Token.spacing.medium

                MD.Button {
                    text: qsTr("Cancel")
                    mdState.type: MD.Enum.BtText
                    onClicked: removeDialog.close()
                }
                MD.Button {
                    text: qsTr("Delete")
                    mdState.type: MD.Enum.BtFilled
                    onClicked: {
                        Core.uninstallPlugin(removeDialog.pluginId)
                        removeDialog.close()
                    }
                }
            }
        }
    }
}
