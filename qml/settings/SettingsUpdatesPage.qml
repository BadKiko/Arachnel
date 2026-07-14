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
            text: qsTr("Update checks and automatic installs.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
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
                    text: qsTr("Check for updates when loading the catalog")
                    typescale: MD.Token.typescale.body_large
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Compares build dates in the catalog with installed games.")
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
                    text: qsTr("Starts downloads for games with updates when the catalog finishes loading. Per-game opt-out is available in game details.")
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
                text: qsTr("Check for updates")
                icon.name: MD.Token.icon.update
                mdState.type: MD.Enum.BtOutlined
                onClicked: Core.checkUpdates()
            }
        }
    }
}
