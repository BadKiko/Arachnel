import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property string stepId
    signal nextRequested()

    Layout.fillWidth: true
    Layout.leftMargin: MD.Token.spacing.large
    Layout.rightMargin: MD.Token.spacing.large
    spacing: MD.Token.spacing.medium
    visible: root.stepId === "updates" || root.stepId === "proton" || root.stepId === "done"

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.stepId === "updates"
        spacing: MD.Token.spacing.medium
        MD.Label { text: qsTr("Updates"); typescale: MD.Token.typescale.headline_small }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Recommended defaults — change anytime in Settings → Updates.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
        Repeater {
            model: [
                {
                    title: qsTr("Check for game updates"),
                    body: qsTr("Notify you when a newer build is available."),
                    checked: Core.settings.autoCheckUpdates,
                    update: function(value) { Core.settings.autoCheckUpdates = value }
                },
                {
                    title: qsTr("Check for Arachnel updates"),
                    body: qsTr("Check for new Arachnel versions automatically."),
                    checked: Core.settings.autoCheckAppUpdates,
                    update: function(value) { Core.settings.autoCheckAppUpdates = value }
                }
            ]
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    MD.Label {
                        Layout.fillWidth: true
                        text: modelData.title
                        typescale: MD.Token.typescale.body_large
                    }
                    MD.Label {
                        Layout.fillWidth: true
                        text: modelData.body
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_small
                    }
                }
                MD.Switch {
                    checked: modelData.checked
                    onToggled: modelData.update(checked)
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.stepId === "proton"
        spacing: MD.Token.spacing.small
        MD.Label { text: qsTr("Proton (Linux)"); typescale: MD.Token.typescale.headline_small }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Windows games need Proton on Linux. Install it now or later in Settings → Launch.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
        MD.Label {
            Layout.fillWidth: true
            visible: Core.protonReady
            text: qsTr("Proton ready: %1").arg(Core.protonVersion)
            color: MD.Token.color.primary
            typescale: MD.Token.typescale.body_medium
        }
        MD.Label {
            Layout.fillWidth: true
            visible: Core.protonDownloadInProgress
            text: Core.protonDownloadStatus.length
                  ? (Core.protonDownloadStatus + " (" + Core.protonDownloadProgress + "%)")
                  : qsTr("Downloading Proton… %1%").arg(Core.protonDownloadProgress)
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_small
        }
        MD.Button {
            Layout.fillWidth: true
            mdState.type: MD.Enum.BtFilled
            enabled: !Core.protonDownloadInProgress
            text: Core.protonReady ? qsTr("Proton already installed")
                  : (Core.protonLatestRelease.length ? qsTr("Download Proton-GE %1").arg(Core.protonLatestRelease)
                                                     : qsTr("Download Proton-GE"))
            onClicked: {
                if (!Core.protonReady)
                    Core.downloadProtonGe()
            }
        }
        MD.Button {
            Layout.fillWidth: true
            mdState.type: MD.Enum.BtText
            text: qsTr("I'll do this later")
            enabled: !Core.protonDownloadInProgress
            onClicked: root.nextRequested()
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.stepId === "done"
        spacing: MD.Token.spacing.small
        MD.Label { text: qsTr("You're all set"); typescale: MD.Token.typescale.headline_small }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Open Catalog to browse games. Change language, storage, and plugins anytime in Settings.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Tip: with a plugin installed, Install runs automatically after download. Catalog-only setups need a manual Install step.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
        }
    }
}
