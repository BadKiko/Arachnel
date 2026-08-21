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

    readonly property var catalogPlugins: Core.pluginCatalog ? Core.pluginCatalog.plugins : []

    readonly property var installedPlugins: {
        const rows = (root.pluginRows || []).slice()
        const catalog = {}
        const list = root.catalogPlugins
        for (let i = 0; i < list.length; ++i)
            catalog[list[i].id] = list[i]

        rows.sort(function (a, b) {
            const ca = catalog[a.pluginId]
            const cb = catalog[b.pluginId]
            const ra = !!(ca && ca.recommended)
            const rb = !!(cb && cb.recommended)
            if (ra !== rb)
                return ra ? -1 : 1
            return (a.name || "").localeCompare(b.name || "")
        })

        return rows.map(function (row) {
            const cat = catalog[row.pluginId] || {}
            return {
                id: row.pluginId,
                pluginId: row.pluginId,
                name: cat.name || row.name,
                description: cat.description || row.description || "",
                iconName: cat.iconName || row.iconName || "extension",
                version: row.pluginVersion,
                pluginVersion: row.pluginVersion,
                repository: row.repositoryUrl || cat.repository || "",
                repositoryUrl: row.repositoryUrl || cat.repository || "",
                loaded: row.loaded !== false
            }
        })
    }

    function reloadPlugins() {
        pluginRows = Core.pluginEntries()
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
            visible: root.installedPlugins.length === 0
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
                        name: MD.Token.icon.extension
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
                    text: qsTr("Open the plugin store and install a plugin to browse games.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }
            }
        }

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.installedPlugins.length > 0
            implicitHeight: listCol.implicitHeight + MD.Token.spacing.small * 2
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: listCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.small
                spacing: 0

                Repeater {
                    model: root.installedPlugins

                    OfficialPluginCard {
                        required property var modelData
                        required property int index
                        plugin: modelData
                        installed: true
                        loaded: modelData.loaded
                        showSource: false
                        showVersion: true
                        showDivider: index < root.installedPlugins.length - 1
                        onUninstallClicked: {
                            removeDialog.pluginId = modelData.pluginId
                            removeDialog.pluginName = modelData.name
                            removeDialog.open()
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
