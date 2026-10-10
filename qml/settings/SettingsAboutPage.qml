import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large

    readonly property string platformLabel: {
        switch (Qt.platform.os) {
        case "windows":
            return qsTr("Windows")
        case "linux":
            return qsTr("Linux")
        case "osx":
            return qsTr("macOS")
        default:
            return Qt.platform.os
        }
    }

    contentWidth: width
    contentHeight: body.implicitHeight + MD.Token.spacing.extra_large
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    SettingsColumn {
        id: body
        margin: root.contentMargin

        // App identity, like a hero card.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: heroRow.implicitHeight + 2 * MD.Token.spacing.large
            radius: 28
            color: MD.Token.color.primary_container

            RowLayout {
                id: heroRow
                anchors.fill: parent
                anchors.margins: MD.Token.spacing.large
                spacing: MD.Token.spacing.large

                ShapeIcon {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 72
                    kind: "cookie9"
                    color: MD.Token.color.primary
                    iconColor: MD.Token.color.on_primary

                    SpiderWebMark {
                        anchors.centerIn: parent
                        width: 40
                        height: 40
                        strokeColor: MD.Token.color.on_primary
                        strokeWidth: 1.8
                        rings: 3
                        spokes: 8
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    DisplayTitle {
                        size: 30
                        color: MD.Token.color.on_primary_container
                        text: Qt.application.name.length ? Qt.application.name : "Arachnel"
                    }
                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Browse catalogs, download games, and launch from your library.")
                        color: MD.Token.color.on_primary_container
                        wrapMode: Text.WordWrap
                        typescale: MD.Token.typescale.body_medium
                    }
                }
            }
        }

        SettingsGroup {
            title: qsTr("Application")

            SettingsRow {
                title: qsTr("Version")
                iconName: MD.Token.icon.tag

                MD.Label {
                    text: Qt.application.version.length ? ("v" + Qt.application.version) : qsTr("Unknown")
                    typescale: MD.Token.typescale.title_medium
                    color: MD.Token.color.on_surface_variant
                }
            }

            SettingsRow {
                title: qsTr("Platform")
                iconName: MD.Token.icon.computer

                MD.Label {
                    text: root.platformLabel
                    typescale: MD.Token.typescale.title_medium
                    color: MD.Token.color.on_surface_variant
                }
            }
        }

        SettingsGroup {
            title: qsTr("Danger zone")

            SettingsRow {
                title: qsTr("Delete application data")
                titleColor: MD.Token.color.error
                subtitle: qsTr("Deletes settings, download history, caches, plugins, and Proton from the app folder. Game files on your disks stay. Arachnel will quit afterward.")
                iconName: MD.Token.icon.delete_forever

                below: [
                    MD.Label {
                        Layout.fillWidth: true
                        text: Core.applicationDataPath()
                        wrapMode: Text.WrapAnywhere
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_medium
                    },
                    MD.Button {
                        mdState.type: MD.Enum.BtOutlined
                        text: qsTr("Delete application data…")
                        onClicked: clearDataDialog.open()
                    }
                ]
            }
        }
    }

    MD.Dialog {
        id: clearDataDialog
        parent: Overlay.overlay
        modal: true
        title: qsTr("Delete application data?")

        contentItem: ColumnLayout {
            spacing: MD.Token.spacing.medium
            width: parent ? parent.width : implicitWidth

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("This cannot be undone. Settings, plugins, caches, and library records will be removed. Game files on disk stay in place.")
                wrapMode: Text.WordWrap
                typescale: MD.Token.typescale.body_medium
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
                    onClicked: clearDataDialog.close()
                }

                MD.Button {
                    mdState.type: MD.Enum.BtFilled
                    text: qsTr("Delete and quit")
                    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                    onClicked: {
                        clearDataDialog.close()
                        Core.clearApplicationData()
                    }
                }
            }
        }
    }
}
