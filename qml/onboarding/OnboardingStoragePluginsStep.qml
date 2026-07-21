import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property string stepId
    required property var pluginRows
    required property var isPluginInstalled
    signal nextRequested()

    Layout.fillWidth: true
    Layout.leftMargin: MD.Token.spacing.large
    Layout.rightMargin: MD.Token.spacing.large
    spacing: MD.Token.spacing.small
    visible: root.stepId === "storage" || root.stepId === "plugins"

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.stepId === "storage"
        spacing: MD.Token.spacing.small
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Game library folder")
            typescale: MD.Token.typescale.headline_small
        }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Choose where games are installed. Downloads go to a subfolder on the same drive.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
        Repeater {
            model: Core.settings.storageLibraries
            Rectangle {
                required property string libraryId
                required property string label
                required property string path
                required property bool isDefault
                Layout.fillWidth: true
                radius: MD.Token.shape.corner.medium
                color: isDefault ? MD.Token.color.secondary_container : MD.Token.color.surface_container
                border.width: 1
                border.color: isDefault ? MD.Token.color.primary : MD.Token.color.outline_variant
                implicitHeight: libraryRow.implicitHeight + MD.Token.spacing.small * 2
                MouseArea {
                    anchors.fill: parent
                    onClicked: Core.settings.storageLibraries.setDefaultLibrary(libraryId)
                }
                RowLayout {
                    id: libraryRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.small
                    ColumnLayout {
                        Layout.fillWidth: true
                        RowLayout {
                            Layout.fillWidth: true
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
        MD.Button {
            Layout.fillWidth: true
            mdState.type: MD.Enum.BtOutlined
            text: qsTr("Choose folder…")
            icon.name: MD.Token.icon.folder_open
            onClicked: {
                const folder = Core.browseStorageFolder()
                if (!folder.length)
                    return
                const id = Core.settings.storageLibraries.addLibrary(folder)
                if (id.length)
                    Core.settings.storageLibraries.setDefaultLibrary(id)
            }
        }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Or keep the default path already listed above.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.stepId === "plugins"
        spacing: MD.Token.spacing.small
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Source plugins")
            typescale: MD.Token.typescale.headline_small
        }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Plugins enable automatic install and Play (e.g. FreeTP). Without one, you can still browse catalogs and install manually.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
        Rectangle {
            Layout.fillWidth: true
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: officialColumn.implicitHeight + MD.Token.spacing.medium * 2
            ColumnLayout {
                id: officialColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small
                RowLayout {
                    Layout.fillWidth: true
                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Official plugins")
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
                    visible: Core.pluginCatalog && Core.pluginCatalog.loading
                    text: qsTr("Loading official plugins…")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                }
                MD.Label {
                    Layout.fillWidth: true
                    visible: Core.pluginCatalog && Core.pluginCatalog.error.length > 0
                    text: Core.pluginCatalog ? Core.pluginCatalog.error : ""
                    color: MD.Token.color.error
                    wrapMode: Text.WordWrap
                    typescale: MD.Token.typescale.body_small
                }
                MD.Label {
                    Layout.fillWidth: true
                    visible: Core.pluginCatalog && !Core.pluginCatalog.loading
                             && Core.pluginCatalog.error.length === 0
                             && Core.pluginCatalog.plugins.length === 0
                    text: qsTr("No official plugins available for this platform.")
                    color: MD.Token.color.on_surface_variant
                    wrapMode: Text.WordWrap
                    typescale: MD.Token.typescale.body_small
                }
                Repeater {
                    model: Core.pluginCatalog ? Core.pluginCatalog.plugins : []
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        readonly property bool installed: root.isPluginInstalled(modelData.id)
                        readonly property bool thisInstalling: Core.pluginCatalog
                                                              && Core.pluginCatalog.installing
                                                              && Core.pluginCatalog.installingPluginId === modelData.id
                        ColumnLayout {
                            Layout.fillWidth: true
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
                            MD.Label {
                                Layout.fillWidth: true
                                text: qsTr("v%1").arg(modelData.version)
                                color: MD.Token.color.primary
                                typescale: MD.Token.typescale.label_small
                            }
                        }
                        MD.Button {
                            mdState.type: installed ? MD.Enum.BtText : MD.Enum.BtFilledTonal
                            text: installed ? qsTr("Installed") : (thisInstalling ? qsTr("Installing…") : qsTr("Install"))
                            enabled: !installed && !(Core.pluginCatalog && Core.pluginCatalog.installing)
                            onClicked: Core.installOfficialPlugin(modelData.id)
                        }
                    }
                }
                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Or install a plugin file you already have.")
                    wrapMode: Text.WordWrap
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                }
            }
        }
        Repeater {
            model: root.pluginRows
            Rectangle {
                required property var modelData
                Layout.fillWidth: true
                radius: MD.Token.shape.corner.medium
                color: MD.Token.color.surface_container
                border.width: 1
                border.color: MD.Token.color.outline_variant
                implicitHeight: installedRow.implicitHeight + MD.Token.spacing.small * 2
                RowLayout {
                    id: installedRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.small
                    ColumnLayout {
                        Layout.fillWidth: true
                        MD.Label { text: modelData.name; typescale: MD.Token.typescale.title_small }
                        MD.Label {
                            text: qsTr("v%1 · %2").arg(modelData.pluginVersion).arg(modelData.pluginId)
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.label_small
                        }
                    }
                    MD.Switch {
                        checked: modelData.sourceEnabled
                        onToggled: Core.sources.setSourceEnabled(modelData.pluginId, checked)
                    }
                }
            }
        }
        MD.Button {
            Layout.fillWidth: true
            mdState.type: MD.Enum.BtFilled
            text: qsTr("Install plugin…")
            onClicked: Core.browsePluginArach()
        }
        MD.Button {
            Layout.fillWidth: true
            mdState.type: MD.Enum.BtText
            text: qsTr("Skip for now")
            onClicked: root.nextRequested()
        }
        MD.Label {
            Layout.fillWidth: true
            visible: Core.lastPluginError.length > 0
            text: Core.lastPluginError
            color: MD.Token.color.error
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_small
        }
    }
}
