import QtQuick
import QtQuick.Layouts
import Arachnel.Core 1.0
import Qcm.Material as MD

MD.ElevationRectangle {
    id: root

    required property var page

    readonly property bool showPanel: page.onLinux && page.installed && page.gameId.length > 0

    property var containerInfo: ({})

    Layout.fillWidth: true
    visible: showPanel
    Layout.preferredHeight: showPanel ? (runtimeCol.implicitHeight + 2 * MD.Token.spacing.medium) : 0
    radius: MD.Token.shape.corner.large
    color: MD.Token.color.surface_container_low
    elevation: MD.Token.elevation.level0
    clip: true

    function refreshContainerInfo() {
        if (!page || !page.gameId || page.gameId.length === 0) {
            root.containerInfo = ({})
            return
        }
        root.containerInfo = Core.gameRuntimeContainerInfo(page.gameId)
    }

    Component.onCompleted: refreshContainerInfo()

    Connections {
        target: root.page
        function onGameIdChanged() { root.refreshContainerInfo() }
        function onDetailsRevisionChanged() { root.refreshContainerInfo() }
        function onInstalledChanged() { root.refreshContainerInfo() }
    }

    Connections {
        target: Core
        function onRuntimeSetupChanged() {
            if (!Core.runtimeSetupInProgress)
                root.refreshContainerInfo()
        }
    }

    ColumnLayout {
        id: runtimeCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: MD.Token.spacing.medium
        spacing: MD.Token.spacing.small

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Runtime container")
            typescale: MD.Token.typescale.title_small
        }

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Proton prefix and redistributables for this game (Linux only).")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.preferredWidth: 120
                Layout.alignment: Qt.AlignTop
                text: qsTr("Container")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
            }

            MD.Label {
                Layout.fillWidth: true
                text: root.containerInfo.containerPath || "—"
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 3
                elide: Text.ElideRight
            }

            MD.IconButton {
                Layout.alignment: Qt.AlignTop
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.folder_open
                enabled: !!(root.containerInfo.containerPath)
                onClicked: Core.openGameRuntimeContainer(page.gameId)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.preferredWidth: 120
                Layout.alignment: Qt.AlignTop
                text: qsTr("Prefix")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
            }

            MD.Label {
                Layout.fillWidth: true
                text: {
                    const path = root.containerInfo.prefixPath || ""
                    if (!path.length)
                        return "—"
                    const exists = root.containerInfo.prefixExists === true
                    return exists ? path : qsTr("%1 (not created yet)").arg(path)
                }
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 3
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small
            visible: !!(root.containerInfo.steamAppId)

            MD.Label {
                Layout.preferredWidth: 120
                text: qsTr("Steam App ID")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
            }

            MD.Label {
                Layout.fillWidth: true
                text: root.containerInfo.steamAppId || "—"
                typescale: MD.Token.typescale.body_medium
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.topMargin: MD.Token.spacing.extra_small
            text: {
                const total = root.containerInfo.totalCount ?? 0
                const done = root.containerInfo.installedCount ?? 0
                if (total <= 0)
                    return qsTr("No runtime dependencies detected for this game.")
                return qsTr("Dependencies: %1 / %2 installed").arg(done).arg(total)
            }
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: root.containerInfo.dependencies ?? []

            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: MD.Token.spacing.small

                MD.Label {
                    Layout.fillWidth: true
                    text: modelData.label || modelData.depotId || ""
                    typescale: MD.Token.typescale.body_medium
                    elide: Text.ElideRight
                }

                MD.Label {
                    text: modelData.installed ? qsTr("Installed") : qsTr("Missing")
                    color: modelData.installed
                           ? MD.Token.color.primary
                           : MD.Token.color.error
                    typescale: MD.Token.typescale.label_medium
                }
            }
        }
    }
}
