import QtQuick
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.Dialog {
    id: root

    property bool immediateCrash: false

    parent: Overlay.overlay
    modal: true
    title: qsTr("Application crashed")
    standardButtons: Dialog.NoButton

    width: Math.min(620, parent ? parent.width - MD.Token.spacing.large * 2 : 620)

    onClosed: Core.dismissPendingCrashReport()

    CrashReportPanel {
        width: root.width - root.horizontalPadding * 2
        showHeader: false
        showSubtitle: true
        immediateCrash: root.immediateCrash
        onDismissRequested: root.close()
        onIssueRequested: root.close()
    }
}
