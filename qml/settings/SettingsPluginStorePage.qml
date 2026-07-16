import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property var pluginRows: []

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
            text: qsTr("Official plugins from the Arachnel catalog. Install adds them to your plugins folder.")
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
                text: qsTr("Available")
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
                     && Core.pluginCatalog.plugins.length === 0
            text: qsTr("No official plugins available for this platform.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_small
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            Repeater {
                model: Core.pluginCatalog ? Core.pluginCatalog.plugins : []

                Rectangle {
                    required property var modelData

                    Layout.fillWidth: true
                    radius: MD.Token.shape.corner.large
                    color: MD.Token.color.surface_container
                    border.width: 1
                    border.color: MD.Token.color.outline_variant
                    implicitHeight: officialCard.implicitHeight + MD.Token.spacing.medium * 2

                    readonly property bool installed: root.isPluginInstalled(modelData.id)
                    readonly property bool thisInstalling: Core.pluginCatalog
                                                          && Core.pluginCatalog.installing
                                                          && Core.pluginCatalog.installingPluginId === modelData.id

                    ColumnLayout {
                        id: officialCard
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
                                    maximumLineCount: 3
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
                                text: installed ? qsTr("Installed")
                                      : (thisInstalling ? qsTr("Installing…") : qsTr("Install"))
                                enabled: !installed && !(Core.pluginCatalog && Core.pluginCatalog.installing)
                                onClicked: Core.installOfficialPlugin(modelData.id)
                            }
                        }
                    }
                }
            }
        }
    }
}
