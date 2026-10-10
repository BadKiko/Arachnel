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
    contentHeight: body.implicitHeight + MD.Token.spacing.extra_large
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    SettingsColumn {
        id: body
        margin: root.contentMargin

        SettingsGroup {
            title: qsTr("Installed")
            description: Messages.settingsPluginsDesc

            SettingsRow {
                visible: root.installedPlugins.length === 0
                title: qsTr("No plugins installed")
                subtitle: qsTr("Open the plugin store and install a plugin to browse games.")
                iconName: MD.Token.icon.extension
            }

            Repeater {
                model: root.installedPlugins

                delegate: SettingsRow {
                    required property var modelData
                    title: modelData.name
                    subtitle: {
                        const parts = []
                        if ((modelData.version || "").length)
                            parts.push("v" + modelData.version)
                        if (!modelData.loaded)
                            parts.push(qsTr("not loaded"))
                        if ((modelData.description || "").length)
                            parts.push(modelData.description)
                        return parts.join(" · ")
                    }
                    iconName: MD.Token.icon[modelData.iconName] ?? MD.Token.icon.extension
                    titleColor: modelData.loaded ? MD.Token.color.on_surface : MD.Token.color.error

                    MD.IconButton {
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.delete
                        Accessible.name: qsTr("Remove plugin?")
                        onClicked: {
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
            Layout.leftMargin: MD.Token.spacing.medium
            visible: Core.lastPluginError.length > 0
            text: Core.lastPluginError
            color: MD.Token.color.error
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        SettingsGroup {
            title: qsTr("Get more")

            SettingsRow {
                title: qsTr("Plugin store")
                subtitle: qsTr("Official and community plugins.")
                iconName: MD.Token.icon.storefront
                clickable: true
                showChevron: true
                onClicked: root.openStoreRequested()
            }

            SettingsRow {
                title: qsTr("Install from file…")
                iconName: MD.Token.icon.upload_file
                clickable: true
                showChevron: true
                onClicked: Core.browsePluginArach()
            }

            SettingsRow {
                title: qsTr("Open folder")
                iconName: MD.Token.icon.folder_open
                clickable: true
                showChevron: true
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
