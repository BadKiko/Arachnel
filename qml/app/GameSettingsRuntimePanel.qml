import QtQuick
import QtQuick.Layouts
import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    required property var page
    implicitHeight: runtimePanel.implicitHeight

        MD.ElevationRectangle {
            id: runtimePanel
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: page.onLinux && page.installed && page.gameId.length > 0
            Layout.preferredHeight: runtimeCol.implicitHeight + 2 * MD.Token.spacing.medium
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container_low
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: runtimeCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small

                readonly property var containerInfo: {
                    const _rev = page.detailsRevision
                    return page.gameId.length ? Core.gameRuntimeContainerInfo(page.gameId) : ({})
                }

                MD.Label {
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
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        Layout.preferredWidth: 140
                        text: qsTr("Container")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: runtimeCol.containerInfo.containerPath || "—"
                        typescale: MD.Token.typescale.body_medium
                        elide: Text.ElideMiddle
                    }

                    MD.IconButton {
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.folder_open
                        enabled: !!(runtimeCol.containerInfo.containerPath)
                        onClicked: Core.openGameRuntimeContainer(page.gameId)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        Layout.preferredWidth: 140
                        text: qsTr("Prefix")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: {
                            const path = runtimeCol.containerInfo.prefixPath || ""
                            if (!path.length)
                                return "—"
                            const exists = runtimeCol.containerInfo.prefixExists === true
                            return exists ? path : qsTr("%1 (not created yet)").arg(path)
                        }
                        typescale: MD.Token.typescale.body_medium
                        elide: Text.ElideMiddle
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium
                    visible: !!(runtimeCol.containerInfo.steamAppId)

                    MD.Label {
                        Layout.preferredWidth: 140
                        text: qsTr("Steam App ID")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: runtimeCol.containerInfo.steamAppId || "—"
                        typescale: MD.Token.typescale.body_medium
                    }
                }

                MD.Label {
                    Layout.fillWidth: true
                    Layout.topMargin: MD.Token.spacing.small
                    text: {
                        const total = runtimeCol.containerInfo.totalCount ?? 0
                        const done = runtimeCol.containerInfo.installedCount ?? 0
                        if (total <= 0)
                            return qsTr("No runtime dependencies detected for this game.")
                        return qsTr("Dependencies: %1 / %2 installed").arg(done).arg(total)
                    }
                    typescale: MD.Token.typescale.body_medium
                }

                Repeater {
                    model: runtimeCol.containerInfo.dependencies ?? []

                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.medium

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
}
