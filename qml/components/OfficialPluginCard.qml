import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Qcm.Material as MD

ColumnLayout {
    id: root

    property var plugin: ({})
    property bool installed: false
    property bool installing: false
    property bool catalogBusy: false
    property bool showDivider: false
    property bool showSource: true
    property bool showVersion: false
    property bool loaded: true

    signal installClicked
    signal uninstallClicked
    signal sourceClicked(string url)

    readonly property string pluginId: plugin.id || plugin.pluginId || ""
    readonly property string iconName: plugin.iconName || "extension"
    readonly property string versionLabel: {
        const v = plugin.version || plugin.pluginVersion || ""
        return v.length ? qsTr("v%1").arg(v) : ""
    }
    readonly property string sourceUrl: plugin.repository || plugin.repositoryUrl || ""
    readonly property string displayName: plugin.name || ""
    readonly property string displayBlurb: plugin.description || ""

    Layout.fillWidth: true
    spacing: 0

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
            Layout.alignment: Qt.AlignTop
            radius: MD.Token.shape.corner.full
            color: MD.Token.color.surface_container_highest
            elevation: MD.Token.elevation.level0

            MD.Icon {
                anchors.centerIn: parent
                name: root.iconName
                size: 20
                color: MD.Token.color.on_surface_variant
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.extra_small

            MD.Label {
                Layout.fillWidth: true
                text: root.displayName
                typescale: MD.Token.typescale.body_large
                elide: Text.ElideRight
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
                visible: root.showVersion && root.versionLabel.length > 0
                text: root.versionLabel
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_small
            }

            MD.Label {
                Layout.fillWidth: true
                visible: root.displayBlurb.length > 0
                text: root.displayBlurb
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_small
            }
        }

        MD.IconButton {
            visible: root.showSource && root.sourceUrl.length > 0
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.open_in_new
            Accessible.name: qsTr("Source code")
            onClicked: root.sourceClicked(root.sourceUrl)
        }

        MD.IconButton {
            visible: root.installed
            mdState.type: MD.Enum.IBtStandard
            icon.name: MD.Token.icon.delete
            Accessible.name: qsTr("Delete")
            onClicked: root.uninstallClicked()
        }

        MD.Button {
            visible: !root.installed
            mdState.type: MD.Enum.BtFilledTonal
            text: root.installing ? qsTr("Installing…") : qsTr("Install")
            icon.name: MD.Token.icon.download
            enabled: !root.catalogBusy
            onClicked: root.installClicked()
        }
    }

    MD.Divider {
        Layout.fillWidth: true
        visible: root.showDivider
    }
}
