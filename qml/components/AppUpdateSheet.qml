import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal
    dismissOnDragDown: true

    property string pendingVersion: ""

    function openForVersion(version) {
        pendingVersion = version || (Core.appUpdater ? Core.appUpdater.latestVersion : "")
        if (!root.opened)
            open()
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: qsTr("Update available")
            typescale: MD.Token.typescale.headline_medium
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: qsTr("Arachnel %1 is ready to install. Update now to get the latest fixes and features.")
                      .arg(root.pendingVersion.length ? root.pendingVersion
                                                       : (Core.appUpdater ? Core.appUpdater.latestVersion : ""))
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: Core.appUpdater && Core.appUpdater.currentVersion.length > 0
            text: qsTr("Current version: %1").arg(Core.appUpdater ? Core.appUpdater.currentVersion : "")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtText
                text: qsTr("Later")
                enabled: !(Core.appUpdater && Core.appUpdater.downloading)
                onClicked: root.close()
            }

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtOutlined
                text: qsTr("Release page")
                enabled: !(Core.appUpdater && Core.appUpdater.downloading)
                onClicked: {
                    if (Core.appUpdater)
                        Core.appUpdater.openReleasePage()
                }
            }

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtFilled
                text: qsTr("Update now")
                enabled: Core.appUpdater && Core.appUpdater.updateAvailable
                         && !Core.appUpdater.checking && !Core.appUpdater.downloading
                onClicked: {
                    if (Core.appUpdater)
                        Core.appUpdater.downloadAndInstall()
                    root.close()
                }
            }
        }
    }
}
