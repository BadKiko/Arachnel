import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: qsTr("Check for game updates and new Arachnel versions.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            text: qsTr("Games")
            typescale: MD.Token.typescale.title_small
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.medium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Check for updates on launch")
                    typescale: MD.Token.typescale.body_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Shows once at startup when a newer build is available in the catalog.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                    wrapMode: Text.WordWrap
                }
            }

            MD.Switch {
                checked: Core.settings.autoCheckUpdates
                onToggled: Core.settings.autoCheckUpdates = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.medium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Install updates automatically on launch")
                    typescale: MD.Token.typescale.body_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Downloads updates once at startup. You can turn this off per game.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                    wrapMode: Text.WordWrap
                }
            }

            MD.Switch {
                checked: Core.settings.autoInstallUpdates
                onToggled: Core.settings.autoInstallUpdates = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.small

            MD.Button {
                text: qsTr("Check for game updates")
                icon.name: MD.Token.icon.update
                mdState.type: MD.Enum.BtOutlined
                onClicked: Core.checkUpdates()
            }
        }

        MD.Divider {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            text: qsTr("Arachnel")
            typescale: MD.Token.typescale.title_small
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            text: qsTr("Current version: %1").arg(Core.appUpdater.currentVersion)
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            text: Core.appUpdater.statusText
            wrapMode: Text.WordWrap
            color: Core.appUpdater.lastError.length
                   ? MD.Token.color.error
                   : MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.medium
            visible: Core.appUpdater.downloading

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 6
                clip: true

                Rectangle {
                    anchors.fill: parent
                    radius: 3
                    color: MD.Util.transparent(MD.Token.color.primary, 0.2)
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: parent.width * (Core.appUpdater.downloadProgress / 100)
                    radius: 3
                    color: MD.Token.color.primary
                }
            }

            MD.Label {
                text: Core.appUpdater.downloadProgress + "%"
                typescale: MD.Token.typescale.label_small
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.medium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Check for Arachnel updates on startup")
                    typescale: MD.Token.typescale.body_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Checks for new versions in the background.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                    wrapMode: Text.WordWrap
                }
            }

            MD.Switch {
                checked: Core.settings.autoCheckAppUpdates
                onToggled: Core.settings.autoCheckAppUpdates = checked
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.small
            spacing: MD.Token.spacing.small

            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.small

                MD.Button {
                    Layout.fillWidth: true
                    text: qsTr("Check for Arachnel updates")
                    icon.name: MD.Token.icon.update
                    mdState.type: MD.Enum.BtOutlined
                    enabled: !Core.appUpdater.checking && !Core.appUpdater.downloading
                    onClicked: Core.appUpdater.checkForUpdates(true)
                }

                MD.Button {
                    Layout.fillWidth: true
                    text: qsTr("Download and install")
                    icon.name: MD.Token.icon.download
                    mdState.type: MD.Enum.BtFilled
                    visible: Core.appUpdater.updateAvailable
                    enabled: !Core.appUpdater.checking && !Core.appUpdater.downloading
                    onClicked: Core.appUpdater.downloadAndInstall()
                }
            }

            MD.Button {
                Layout.fillWidth: true
                text: qsTr("Open release page")
                mdState.type: MD.Enum.BtText
                onClicked: Core.appUpdater.openReleasePage()
            }
        }
    }
}
