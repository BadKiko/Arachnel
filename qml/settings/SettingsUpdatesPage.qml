import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large

    contentWidth: width
    contentHeight: body.implicitHeight + MD.Token.spacing.extra_large
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    SettingsColumn {
        id: body
        margin: root.contentMargin

        SettingsGroup {
            title: qsTr("Games")
            description: qsTr("Check for game updates and new Arachnel versions.")

            SettingsSwitchRow {
                title: qsTr("Check for updates on launch")
                subtitle: qsTr("Shows once at startup when a newer build is available in the catalog.")
                checked: Core.settings.autoCheckUpdates
                onToggled: function (on) { Core.settings.autoCheckUpdates = on }
            }

            SettingsSwitchRow {
                title: qsTr("Install updates automatically on launch")
                subtitle: qsTr("Downloads updates once at startup. You can turn this off per game.")
                checked: Core.settings.autoInstallUpdates
                onToggled: function (on) { Core.settings.autoInstallUpdates = on }
            }

            SettingsRow {
                title: qsTr("Check for game updates")
                iconName: MD.Token.icon.update
                clickable: true
                showChevron: true
                onClicked: Core.checkUpdates()
            }
        }

        SettingsGroup {
            title: qsTr("Arachnel")

            SettingsRow {
                title: qsTr("Current version: %1").arg(Core.appUpdater.currentVersion)
                subtitle: Core.appUpdater.statusText
                iconName: MD.Token.icon.info

                below: [
                    MD.LinearIndicator {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        visible: Core.appUpdater.downloading
                        indeterminate: false
                        wavy: true
                        from: 0
                        to: 100
                        value: Core.appUpdater.downloadProgress
                        strokeWidth: 4
                    },
                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            text: qsTr("Check for Arachnel updates")
                            icon.name: MD.Token.icon.update
                            mdState.type: MD.Enum.BtFilledTonal
                            enabled: !Core.appUpdater.checking && !Core.appUpdater.downloading
                            onClicked: Core.appUpdater.checkForUpdates(true)
                        }
                        MD.Button {
                            text: qsTr("Download and install")
                            icon.name: MD.Token.icon.download
                            mdState.type: MD.Enum.BtFilled
                            visible: Core.appUpdater.updateAvailable
                            enabled: !Core.appUpdater.checking && !Core.appUpdater.downloading
                            onClicked: Core.appUpdater.downloadAndInstall()
                        }
                        MD.Button {
                            text: qsTr("Open release page")
                            mdState.type: MD.Enum.BtText
                            onClicked: Core.appUpdater.openReleasePage()
                        }
                    }
                ]
            }

            SettingsSwitchRow {
                title: qsTr("Check for Arachnel updates on startup")
                subtitle: qsTr("Checks for new versions in the background.")
                checked: Core.settings.autoCheckAppUpdates
                onToggled: function (on) { Core.settings.autoCheckAppUpdates = on }
            }

            SettingsSwitchRow {
                title: qsTr("Include pre-releases")
                subtitle: qsTr("Offer beta and RC builds from GitHub.")
                checked: Core.settings.includeAppPreReleases
                onToggled: function (on) { Core.settings.includeAppPreReleases = on }
            }
        }
    }
}
