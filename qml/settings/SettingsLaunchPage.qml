import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large

    readonly property bool onLinux: Qt.platform.os === "linux"

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    Component.onCompleted: {
        if (root.onLinux) {
            Core.refreshProtonLatestRelease()
            Core.refreshAvailableProtons()
        }
    }

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: qsTr("Extra command-line arguments appended to every game launch.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        MD.TextField {
            id: globalArgsField
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            placeholderText: qsTr("Global launch arguments")
            text: Core.settings.globalLaunchArgs
            onEditingFinished: Core.settings.globalLaunchArgs = text
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.onLinux
            text: qsTr("Linux: all games run through Proton (Windows builds).")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.onLinux
            Layout.preferredHeight: protonCol.implicitHeight + 2 * MD.Token.spacing.large
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: protonCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.large
                spacing: MD.Token.spacing.medium

                MD.Label {
                    text: qsTr("Proton runtime")
                    typescale: MD.Token.typescale.title_small
                }

                MD.Label {
                    Layout.fillWidth: true
                    visible: Core.protonReady && Core.protonVersion.length > 0
                    text: qsTr("Default: %1").arg(Core.protonVersion)
                    color: MD.Token.color.primary
                    typescale: MD.Token.typescale.body_medium
                    wrapMode: Text.WordWrap
                }

                MD.Label {
                    Layout.fillWidth: true
                    visible: !Core.protonReady
                    text: Core.protonLatestRelease.length
                          ? qsTr("Required before download: %1").arg(Core.protonLatestRelease)
                          : qsTr("Install Proton-GE before downloading games.")
                    color: MD.Token.color.error
                    typescale: MD.Token.typescale.body_medium
                    wrapMode: Text.WordWrap
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Pick default Proton and drag priority with arrows. Steam installs are detected automatically.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    MD.Button {
                        text: Core.protonLatestRelease.length
                              ? qsTr("Download %1").arg(Core.protonLatestRelease)
                              : qsTr("Download Proton-GE")
                        icon.name: MD.Token.icon.download
                        enabled: !Core.protonDownloadInProgress
                        onClicked: Core.downloadProtonGe()
                    }

                    MD.Button {
                        text: qsTr("Rescan")
                        mdState.type: MD.Enum.BtOutlined
                        onClicked: Core.refreshAvailableProtons()
                    }
                }

                MD.Label {
                    Layout.fillWidth: true
                    visible: Core.protonDownloadInProgress
                    text: Core.protonDownloadStatus + " (" + Core.protonDownloadProgress + "%)"
                    color: MD.Token.color.primary
                    typescale: MD.Token.typescale.body_small
                    wrapMode: Text.WordWrap
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: Core.availableProtons.length > 0
                    spacing: MD.Token.spacing.small

                    Repeater {
                        model: Core.availableProtons

                        Rectangle {
                            required property var modelData

                            Layout.fillWidth: true
                            radius: MD.Token.shape.corner.medium
                            color: modelData.isDefault
                                   ? MD.Token.color.secondary_container
                                   : MD.Token.color.surface_container_high
                            border.width: modelData.isDefault ? 1 : 0
                            border.color: MD.Token.color.primary
                            implicitHeight: protonRow.implicitHeight + MD.Token.spacing.medium * 2

                            MouseArea {
                                anchors.fill: parent
                                onClicked: Core.settings.defaultProtonId = modelData.id
                            }

                            RowLayout {
                                id: protonRow
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: MD.Token.spacing.medium
                                spacing: MD.Token.spacing.small

                                MD.Icon {
                                    visible: modelData.isDefault
                                    name: MD.Token.icon.star
                                    size: 20
                                    color: MD.Token.color.primary
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    MD.Label {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        typescale: MD.Token.typescale.body_large
                                        elide: Text.ElideRight
                                    }

                                    MD.Label {
                                        Layout.fillWidth: true
                                        text: modelData.sourceLabel
                                        color: MD.Token.color.on_surface_variant
                                        typescale: MD.Token.typescale.body_small
                                    }
                                }

                                MD.IconButton {
                                    mdState.type: MD.Enum.IBtStandard
                                    display: MD.IconButton.TextOnly
                                    text: "↑"
                                    enabled: (modelData.priorityIndex ?? 0) > 0
                                    onClicked: Core.moveProtonInPriority(modelData.id, -1)
                                }

                                MD.IconButton {
                                    mdState.type: MD.Enum.IBtStandard
                                    display: MD.IconButton.TextOnly
                                    text: "↓"
                                    enabled: (modelData.priorityIndex ?? 0) >= 0
                                             && (modelData.priorityIndex ?? 0) < Core.availableProtons.length - 1
                                    onClicked: Core.moveProtonInPriority(modelData.id, 1)
                                }
                            }
                        }
                    }
                }

                MD.Label {
                    Layout.fillWidth: true
                    visible: Core.availableProtons.length === 0 && !Core.protonDownloadInProgress
                    text: qsTr("No Proton found. Download Proton-GE or install Proton in Steam.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
