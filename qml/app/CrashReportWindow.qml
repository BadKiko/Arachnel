import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.ApplicationWindow {
    id: root

    visible: true
    width: 620
    height: 440
    minimumWidth: 520
    minimumHeight: 340
    title: qsTr("Application crashed")
    color: MD.Token.color.surface_container
    flags: customTitleBar ? (Qt.Window | Qt.FramelessWindowHint) : Qt.Window

    readonly property bool customTitleBar: Qt.platform.os === "windows"

    MD.MProp.textColor: MD.MProp.color.on_surface
    MD.MProp.backgroundColor: MD.MProp.color.surface_container

    function finish() {
        Core.dismissPendingCrashReport()
        Qt.quit()
    }

    onClosing: finish()

    Component.onCompleted: {
        Appearance.apply()
        if (!Core.hasPendingCrashReport()) {
            Qt.quit()
            return
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        AppTitleBar {
            Layout.fillWidth: true
            window: root
        }

        MD.Pane {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: MD.Token.spacing.small
            padding: MD.Token.spacing.medium
            radius: MD.Token.shape.corner.extra_large
            backgroundColor: MD.Token.color.surface
            clip: true

            CrashReportPanel {
                anchors.fill: parent
                immediateCrash: true
                showHeader: true
                expandReport: true
                onDismissRequested: root.finish()
            }
        }
    }

    WindowResizeEdges {
        anchors.fill: parent
        window: root
        z: 1000
    }
}
