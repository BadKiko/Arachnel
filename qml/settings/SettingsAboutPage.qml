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
            text: qsTr("Browse catalogs, download games, and launch from your library.")
            color: MD.Token.color.on_surface_variant
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: aboutColumn.implicitHeight + MD.Token.spacing.medium * 2

            ColumnLayout {
                id: aboutColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Application")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_large
                    }

                    MD.Label {
                        text: Qt.application.name.length ? Qt.application.name : "Arachnel"
                        typescale: MD.Token.typescale.body_large
                        horizontalAlignment: Text.AlignRight
                    }
                }

                MD.Divider { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Version")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_large
                    }

                    MD.Label {
                        text: Qt.application.version.length ? ("v" + Qt.application.version) : qsTr("Unknown")
                        typescale: MD.Token.typescale.body_large
                        horizontalAlignment: Text.AlignRight
                    }
                }

                MD.Divider { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Platform")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_large
                    }

                    MD.Label {
                        text: root.platformLabel
                        typescale: MD.Token.typescale.body_large
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: qsTr("Danger zone")
            typescale: MD.Token.typescale.title_small
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: dangerCol.implicitHeight + MD.Token.spacing.medium * 2

            ColumnLayout {
                id: dangerCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Delete application data")
                    typescale: MD.Token.typescale.title_small
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Deletes settings, download history, caches, plugins, and Proton from the app folder. Game files on your disks stay. Arachnel will quit afterward.")
                    wrapMode: Text.WordWrap
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: Core.applicationDataPath()
                    wrapMode: Text.WrapAnywhere
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_small
                }

                MD.Button {
                    Layout.fillWidth: true
                    mdState.type: MD.Enum.BtOutlined
                    text: qsTr("Delete application data…")
                    onClicked: clearDataDialog.open()
                }
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
