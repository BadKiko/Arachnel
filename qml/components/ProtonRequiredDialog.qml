import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.Dialog {
    id: root

    parent: Overlay.overlay
    modal: true
    title: qsTr("Proton required")

    signal openLaunchSettings()

    readonly property string releaseName: Core.protonLatestRelease.length
                                        ? Core.protonLatestRelease
                                        : qsTr("latest Proton-GE")

    onAboutToShow: {
        if (Qt.platform.os === "linux")
            Core.refreshProtonLatestRelease()
    }

    contentItem: ColumnLayout {
        spacing: MD.Token.spacing.medium
        width: parent ? parent.width : implicitWidth

        MD.Label {
            Layout.fillWidth: true
            text: Core.protonLatestRelease.length
                  ? qsTr("Games run through Proton on Linux. Install %1 before downloading.")
                        .arg(Core.protonLatestRelease)
                  : qsTr("Games run through Proton on Linux. Install Proton-GE before downloading.")
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        MD.Label {
            Layout.fillWidth: true
            visible: Core.protonVersion.length > 0
            text: qsTr("Currently installed: %1").arg(Core.protonVersion)
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
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
                onClicked: root.close()
            }

            MD.Button {
                mdState.type: MD.Enum.BtOutlined
                text: qsTr("Settings")
                onClicked: {
                    root.close()
                    root.openLaunchSettings()
                }
            }

            MD.Button {
                mdState.type: MD.Enum.BtFilled
                text: Core.protonDownloadInProgress
                      ? qsTr("Downloading…")
                      : qsTr("Download %1").arg(root.releaseName)
                enabled: !Core.protonDownloadInProgress
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: Core.downloadProtonGe()
            }
        }
    }

    Connections {
        target: Core
        function onProtonStateChanged() {
            if (Core.protonReady)
                root.close()
        }
    }
}
