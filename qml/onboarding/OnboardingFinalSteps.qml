import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property string stepId
    property bool hasSource: false
    property string sourceNames: ""
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
            text: qsTr("Recommended defaults - change anytime in Settings → Updates.")
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
            text: qsTr("Proton lets Windows games run on Linux. Install it now or later in Settings → Launch.")
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

        // What the user picked, so the last step confirms it instead of being an empty page.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: summaryCol.implicitHeight + MD.Token.spacing.medium * 2
            radius: MD.Token.shape.corner.medium
            color: MD.Token.color.surface_container
            ColumnLayout {
                id: summaryCol
                anchors.fill: parent
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.extra_small
                Repeater {
                    model: [
                        { label: qsTr("Language"), value: Core.settings.uiLanguage === "ru" ? "Русский" : "English" },
                        { label: qsTr("Games folder"), value: Core.settings.libraryRoot },
                        { label: qsTr("Game source"), value: root.hasSource ? root.sourceNames : qsTr("None yet") }
                    ]
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.medium
                        MD.Label {
                            Layout.preferredWidth: 110
                            text: modelData.label
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }
                        MD.Label {
                            Layout.fillWidth: true
                            text: modelData.value
                            elide: Text.ElideMiddle
                            typescale: MD.Token.typescale.body_medium
                        }
                    }
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.topMargin: MD.Token.spacing.small
            visible: root.hasSource
            text: qsTr("How to get your first game")
            typescale: MD.Token.typescale.title_small
        }
        Repeater {
            model: root.hasSource
                   ? [Messages.onboardingNextCatalog, Messages.onboardingNextDownload, Messages.onboardingNextPlay]
                   : []
            RowLayout {
                required property int index
                required property string modelData
                Layout.fillWidth: true
                spacing: MD.Token.spacing.small
                Rectangle {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    radius: 12
                    color: MD.Token.color.primary_container
                    MD.Label {
                        anchors.centerIn: parent
                        text: String(index + 1)
                        color: MD.Token.color.on_primary_container
                        typescale: MD.Token.typescale.label_medium
                    }
                }
                MD.Label {
                    Layout.fillWidth: true
                    text: modelData
                    wrapMode: Text.WordWrap
                    typescale: MD.Token.typescale.body_medium
                }
            }
        }
        MD.Label {
            Layout.fillWidth: true
            visible: !root.hasSource
            text: qsTr("You have no game source yet, so Catalog will be empty. Add one in Settings → Plugins to see games.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
        MD.Label {
            Layout.fillWidth: true
            Layout.topMargin: MD.Token.spacing.small
            text: Messages.gamePlayabilityNote
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
        }
    }
}
