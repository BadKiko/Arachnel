import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// A plugin in the store as a card: icon in an expressive shape, name, what it does, and one clear
// action - Install, Installing, or Installed (with remove). The source link sits in the corner.
Rectangle {
    id: root

    property var plugin: ({})
    property bool installed: false
    property bool installing: false
    property bool busy: false
    property string shapeKind: "cookie9"

    signal installClicked()
    signal uninstallClicked()
    signal sourceClicked(string url)

    readonly property string sourceUrl: plugin.repository || plugin.repositoryUrl || ""
    readonly property string version: plugin.version || plugin.pluginVersion || ""

    implicitHeight: col.implicitHeight + 2 * MD.Token.spacing.large
    radius: hover.hovered ? 32 : 28
    color: root.installed ? MD.Util.transparent(MD.Token.color.secondary_container, 0.45)
         : (hover.hovered ? MD.Token.color.surface_container_high : MD.Token.color.surface_container)
    Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
    Behavior on color { ColorAnimation { duration: AppMotion.short } }

    HoverHandler { id: hover }

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: MD.Token.spacing.large
        spacing: MD.Token.spacing.medium

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.medium

            ShapeIcon {
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
                kind: root.shapeKind
                iconName: MD.Token.icon[root.plugin.iconName || "extension"] ?? MD.Token.icon.extension
                color: root.installed ? MD.Token.color.primary : MD.Token.color.primary_container
                iconColor: root.installed ? MD.Token.color.on_primary : MD.Token.color.on_primary_container
                iconSize: 26
                active: hover.hovered
                interactive: false
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    Layout.fillWidth: true
                    text: root.plugin.name || ""
                    typescale: MD.Token.typescale.title_large
                    elide: Text.ElideRight
                }
                MD.Label {
                    Layout.fillWidth: true
                    visible: root.version.length > 0 || root.installed
                    text: root.installed
                          ? (root.version.length ? qsTr("Installed · v%1").arg(root.version) : qsTr("Installed"))
                          : "v" + root.version
                    typescale: MD.Token.typescale.label_large
                    color: root.installed ? MD.Token.color.primary : MD.Token.color.on_surface_variant
                }
            }

            MD.IconButton {
                Layout.alignment: Qt.AlignTop
                visible: root.sourceUrl.length > 0
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.open_in_new
                Accessible.name: qsTr("Source code")
                onClicked: root.sourceClicked(root.sourceUrl)

                MD.ToolTip {
                    visible: parent.hovered
                    text: qsTr("Source code")
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.plugin.description || ""
            typescale: MD.Token.typescale.body_medium
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            MD.LinearIndicator {
                Layout.fillWidth: true
                visible: root.installing
                indeterminate: true
                running: visible
                wavy: true
            }

            Item { Layout.fillWidth: true; visible: !root.installing }

            MD.Button {
                visible: !root.installed && !root.installing
                enabled: !root.busy
                text: qsTr("Install")
                icon.name: MD.Token.icon.download
                mdState.type: MD.Enum.BtFilled
                onClicked: root.installClicked()
            }
            MD.Button {
                visible: root.installed
                text: qsTr("Remove")
                icon.name: MD.Token.icon.delete
                mdState.type: MD.Enum.BtText
                onClicked: root.uninstallClicked()
            }
        }
    }
}
