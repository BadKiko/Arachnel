import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Qcm.Material as MD

MD.ApplicationWindow {
    id: root

    width: 560
    height: 460
    minimumWidth: 520
    minimumHeight: 400
    visible: true
    title: qsTr("Arachnel Setup")
    color: MD.Token.color.surface
    flags: Qt.Window | Qt.FramelessWindowHint

    MD.MProp.textColor: MD.MProp.color.on_surface
    MD.MProp.backgroundColor: MD.MProp.color.surface

    Component.onCompleted: {
        Appearance.apply()
        if (Setup.updateMode)
            Qt.callLater(function () { Setup.beginUpdateIfNeeded() })
    }

    SpiderWebMark {
        width: 300
        height: 300
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: -120
        strokeColor: MD.Token.color.primary
        strokeWidth: 2
        opacity: 0.06
        z: 0
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        z: 1

        SetupTitleBar {
            Layout.fillWidth: true
            window: root
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: MD.Token.spacing.large
                spacing: MD.Token.spacing.large

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium
                    visible: Setup.phase === 0

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Choose language")
                        typescale: MD.Token.typescale.headline_small
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Select the installer language.")
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                    }

                    Repeater {
                        model: Setup.availableLanguages()

                        MD.Button {
                            Layout.fillWidth: true
                            text: modelData.label
                            mdState.type: Setup.language === modelData.code
                                         ? MD.Enum.BtFilled
                                         : MD.Enum.BtOutlined
                            onClicked: Setup.language = modelData.code
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small
                    visible: Setup.phase === 1

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Install Arachnel")
                        typescale: MD.Token.typescale.headline_small
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Game launcher with plugin-based sources. This wizard unpacks Arachnel to your computer.")
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        visible: !Setup.hasPayload
                        text: qsTr("No embedded app payload found. Build the installer with run.ps1 --installer.")
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.error
                        typescale: MD.Token.typescale.body_small
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium
                    visible: Setup.phase === 2

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Choose install location")
                        typescale: MD.Token.typescale.title_medium
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.TextField {
                            id: pathField
                            Layout.fillWidth: true
                            text: Setup.installPath
                            placeholderText: qsTr("Install folder")
                            onEditingFinished: Setup.installPath = text
                        }

                        MD.IconButton {
                            mdState.type: MD.Enum.IBtOutlined
                            icon.name: MD.Token.icon.folder_open
                            onClicked: {
                                Setup.browseInstallFolder()
                                pathField.text = Setup.installPath
                            }
                        }
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Shortcuts")
                        typescale: MD.Token.typescale.title_small
                    }

                    MD.CheckBox {
                        text: qsTr("Create desktop shortcut")
                        checked: Setup.createDesktopShortcut
                        onToggled: Setup.createDesktopShortcut = checked
                    }

                    MD.CheckBox {
                        text: qsTr("Create Start Menu shortcut")
                        checked: Setup.createStartMenuShortcut
                        onToggled: Setup.createStartMenuShortcut = checked
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.medium
                    visible: Setup.phase === 3

                    MD.Label {
                        Layout.fillWidth: true
                        text: Setup.updateMode ? qsTr("Updating Arachnel…") : qsTr("Installing…")
                        typescale: MD.Token.typescale.title_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: Setup.updateMode
                              ? qsTr("Please wait while Arachnel is updated. Do not close this window.")
                              : qsTr("Arachnel is being installed on your computer.")
                        visible: Setup.updateMode || Setup.statusText.length === 0
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: Setup.statusText
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                        elide: Text.ElideRight
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: Setup.progress + "%"
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_small
                    }

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
                            width: parent.width * (Setup.progress / 100)
                            radius: 3
                            color: MD.Token.color.primary
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small
                    visible: Setup.phase === 4

                    MD.Icon {
                        name: MD.Token.icon.check_circle
                        size: 40
                        color: MD.Token.color.primary
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: Setup.updateMode ? qsTr("Arachnel is up to date") : qsTr("Arachnel is ready")
                        typescale: MD.Token.typescale.headline_small
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: Setup.installPath
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_small
                        wrapMode: Text.WordWrap
                    }
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    Item { Layout.fillWidth: true }

                    MD.Button {
                        visible: Setup.phase > 0 && Setup.phase < 4 && !Setup.busy && !Setup.updateMode
                        text: qsTr("Back")
                        mdState.type: MD.Enum.BtText
                        onClicked: Setup.phase = Math.max(0, Setup.phase - 1)
                    }

                    MD.Button {
                        visible: Setup.phase === 0
                        text: qsTr("Continue")
                        mdState.type: MD.Enum.BtFilled
                        onClicked: Setup.phase = 1
                    }

                    MD.Button {
                        visible: Setup.phase === 1
                        text: qsTr("Continue")
                        mdState.type: MD.Enum.BtFilled
                        enabled: Setup.hasPayload
                        onClicked: Setup.phase = 2
                    }

                    MD.Button {
                        visible: Setup.phase === 2
                        text: qsTr("Install")
                        mdState.type: MD.Enum.BtFilled
                        enabled: Setup.canInstall
                        onClicked: Setup.startInstall()
                    }

                    MD.Button {
                        visible: Setup.phase === 4 && !Setup.updateMode
                        text: qsTr("Open folder")
                        mdState.type: MD.Enum.BtOutlined
                        onClicked: Setup.openInstallFolder()
                    }

                    MD.Button {
                        visible: Setup.phase === 4 && !Setup.updateMode
                        text: qsTr("Launch")
                        mdState.type: MD.Enum.BtFilledTonal
                        onClicked: {
                            Setup.launchInstalled()
                            root.close()
                        }
                    }

                    MD.Button {
                        visible: Setup.phase === 4 && !Setup.updateMode
                        text: qsTr("Finish")
                        mdState.type: MD.Enum.BtFilled
                        onClicked: root.close()
                    }
                }
            }
        }
    }
}
