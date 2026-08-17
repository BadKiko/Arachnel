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
    readonly property var steamPlugin: {
        const list = root.catalogPlugins
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === "steamidra")
                return list[i]
        }
        return null
    }
    readonly property var extraPlugins: {
        const list = root.catalogPlugins
        const out = []
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id !== "steamidra")
                out.push(list[i])
        }
        return out
    }

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

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Start here")
                typescale: MD.Token.typescale.title_small
            }

            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("Refresh list")
                enabled: !(Core.pluginCatalog && Core.pluginCatalog.loading)
                onClicked: Core.refreshOfficialPlugins()
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: Core.pluginCatalog && Core.pluginCatalog.loading
            text: qsTr("Loading official plugins…")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: Core.pluginCatalog && Core.pluginCatalog.error.length > 0
            text: Core.pluginCatalog ? Core.pluginCatalog.error : ""
            color: MD.Token.color.error
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_small
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: Core.pluginCatalog && !Core.pluginCatalog.loading
                     && Core.pluginCatalog.error.length === 0
                     && root.catalogPlugins.length === 0
            text: qsTr("No official plugins available for this platform.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_small
        }

        OfficialPluginCard {
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: !!root.steamPlugin
            plugin: root.steamPlugin || ({})
            featured: true
            installed: root.steamPlugin ? root.isPluginInstalled(root.steamPlugin.id) : false
            installing: root.steamPlugin ? root.thisInstalling(root.steamPlugin.id) : false
            catalogBusy: root.catalogBusy
            onInstallClicked: Core.installOfficialPlugin("steamidra")
            onUninstallClicked: root.requestUninstall("steamidra", qsTr("Steam"))
            onSourceClicked: function (url) { Core.openExternalUrl(url) }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.extraPlugins.length > 0
            text: qsTr("Other plugins")
            typescale: MD.Token.typescale.title_small
        }

        Repeater {
            model: root.extraPlugins

            OfficialPluginCard {
                required property var modelData
                Layout.leftMargin: contentMargin
                Layout.rightMargin: contentMargin
                plugin: modelData
                installed: root.isPluginInstalled(modelData.id)
                installing: root.thisInstalling(modelData.id)
                catalogBusy: root.catalogBusy
                onInstallClicked: Core.installOfficialPlugin(modelData.id)
                onUninstallClicked: root.requestUninstall(modelData.id, modelData.name)
                onSourceClicked: function (url) { Core.openExternalUrl(url) }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: MD.Token.spacing.medium
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
