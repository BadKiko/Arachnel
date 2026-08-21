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
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                text: Messages.settingsPluginsDesc
                wrapMode: Text.WordWrap
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
            }

            MD.Button {
                Layout.alignment: Qt.AlignTop
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

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.catalogPlugins.length > 0
            implicitHeight: storeCol.implicitHeight + MD.Token.spacing.small * 2
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            elevation: MD.Token.elevation.level0

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
