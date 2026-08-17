import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Qcm.Material as MD

MD.Card {
    id: root

    property var plugin: ({})
    property bool installed: false
    property bool installing: false
    property bool catalogBusy: false
    property bool featured: false

    signal installClicked
    signal uninstallClicked
    signal sourceClicked(string url)

    readonly property string pluginId: plugin.id || plugin.pluginId || ""
    readonly property bool isSteam: pluginId === "steamidra"
    readonly property bool isHero: root.featured || root.isSteam
    readonly property string iconName: plugin.iconName
                                       || (root.isSteam ? "stadia_controller" : "extension")
    readonly property string versionLabel: {
        const v = plugin.version || plugin.pluginVersion || ""
        return v.length ? qsTr("v%1").arg(v) : ""
    }
    readonly property string sourceUrl: plugin.repository || plugin.repositoryUrl || ""
    property bool showSource: true
    property bool loaded: true

    readonly property string displayName: {
        if (root.isSteam)
            return qsTr("Steam")
        return plugin.name || ""
    }

    readonly property string displayBlurb: {
        if (root.isSteam)
            return qsTr("Play and download any DRM-free games.")
        if (pluginId === "freetp")
            return qsTr("Games from FreeTP as torrents and installers.")
        return plugin.description || ""
    }

    Layout.fillWidth: true
    type: root.isHero ? MD.Enum.CardFilled : MD.Enum.CardOutlined
    verticalPadding: root.isHero ? MD.Token.spacing.large : MD.Token.spacing.medium
    horizontalPadding: root.isHero ? MD.Token.spacing.large : MD.Token.spacing.medium

    contentItem: ColumnLayout {
        spacing: MD.Token.spacing.medium

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.medium

            MD.ElevationRectangle {
                Layout.preferredWidth: root.isHero ? 56 : 40
                Layout.preferredHeight: root.isHero ? 56 : 40
                Layout.alignment: Qt.AlignTop
                radius: MD.Token.shape.corner.full
                color: root.isHero ? MD.Token.color.primary_container
                                   : MD.Token.color.surface_container_highest
                elevation: MD.Token.elevation.level0

                MD.Icon {
                    anchors.centerIn: parent
                    name: root.iconName
                    size: root.isHero ? 28 : 20
                    color: root.isHero ? MD.Token.color.on_primary_container
                                       : MD.Token.color.on_surface_variant
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.extra_small

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.displayName
                        typescale: root.isHero ? MD.Token.typescale.title_large
                                               : MD.Token.typescale.title_medium
                        elide: Text.ElideRight
                    }

                    MD.Label {
                        visible: root.versionLabel.length > 0
                        text: root.versionLabel
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_small
                    }
                }

                MD.AssistChip {
                    visible: root.isHero
                    text: root.installed ? qsTr("Main plugin") : qsTr("Get this first")
                    enabled: false
                    elevated: root.isHero
                }

                MD.Label {
                    Layout.fillWidth: true
                    visible: !root.loaded
                    text: qsTr("Not loaded")
                    color: MD.Token.color.error
                    typescale: MD.Token.typescale.label_medium
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: root.displayBlurb
                    wrapMode: Text.WordWrap
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            Item { Layout.fillWidth: true }

            MD.Button {
                visible: root.showSource && root.sourceUrl.length > 0
                mdState.type: MD.Enum.BtText
                text: qsTr("Source code")
                icon.name: MD.Token.icon.open_in_new
                onClicked: root.sourceClicked(root.sourceUrl)
            }

            MD.Button {
                mdState.type: root.installed ? MD.Enum.BtText
                              : (root.isHero ? MD.Enum.BtFilled : MD.Enum.BtFilledTonal)
                text: root.installed ? qsTr("Delete")
                      : (root.installing ? qsTr("Installing…") : qsTr("Install"))
                icon.name: root.installed ? MD.Token.icon.delete : MD.Token.icon.download
                enabled: root.installed || !root.catalogBusy
                onClicked: {
                    if (root.installed)
                        root.uninstallClicked()
                    else
                        root.installClicked()
                }
            }
        }
    }
}
