import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    property bool immediateCrash: false
    property bool showHeader: true
    property bool showSubtitle: !root.showHeader
    property bool expandReport: false

    signal dismissRequested()
    signal issueRequested()

    spacing: MD.Token.spacing.small

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.showHeader
        spacing: MD.Token.spacing.extra_small

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Application crashed")
            typescale: MD.Token.typescale.headline_small
        }

        MD.Label {
            Layout.fillWidth: true
            text: root.immediateCrash
                  ? qsTr("Arachnel has crashed.")
                  : qsTr("Arachnel stopped unexpectedly during the last session.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }
    }

    MD.Label {
        Layout.fillWidth: true
        visible: !root.showHeader && root.showSubtitle
        text: root.immediateCrash
              ? qsTr("Arachnel has crashed.")
              : qsTr("Arachnel stopped unexpectedly during the last session.")
        wrapMode: Text.WordWrap
        color: MD.Token.color.on_surface_variant
        typescale: MD.Token.typescale.body_medium
    }

    MD.Label {
        Layout.fillWidth: true
        text: Core.pendingCrashSummary()
        wrapMode: Text.WordWrap
        color: MD.Token.color.error
        typescale: MD.Token.typescale.title_small
    }

    MD.ElevationRectangle {
        Layout.fillWidth: true
        Layout.fillHeight: root.expandReport
        Layout.preferredHeight: root.expandReport
                            ? -1
                            : Math.min(220, Math.max(112, reportField.contentHeight + 2 * MD.Token.spacing.small))
        Layout.minimumHeight: 112
        radius: MD.Token.shape.corner.large
        color: MD.Token.color.surface_container_low
        elevation: MD.Token.elevation.level0

        ScrollView {
            anchors.fill: parent
            anchors.margins: MD.Token.spacing.small
            clip: true

            TextArea {
                id: reportField
                readOnly: true
                text: Core.pendingCrashDetails()
                wrapMode: TextArea.Wrap
                font.family: "Consolas, Courier New, monospace"
                font.pixelSize: 12
                color: MD.Token.color.on_surface_variant
                background: null
            }
        }
    }

    MD.Label {
        Layout.fillWidth: true
        text: qsTr("Report file: %1").arg(Core.pendingCrashReportPath())
        wrapMode: Text.NoWrap
        elide: Text.ElideMiddle
        color: MD.Token.color.on_surface_variant
        typescale: MD.Token.typescale.label_medium
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.small

        MD.Button {
            mdState.type: MD.Enum.BtText
            text: qsTr("Dismiss")
            onClicked: root.dismissRequested()
        }

        MD.Button {
            mdState.type: MD.Enum.BtText
            text: qsTr("Open folder")
            onClicked: Core.revealPendingCrashReport()
        }

        MD.Button {
            mdState.type: MD.Enum.BtText
            text: qsTr("Copy report")
            onClicked: Core.copyPendingCrashReport()
        }

        Item { Layout.fillWidth: true }

        MD.Button {
            mdState.type: MD.Enum.BtFilled
            text: qsTr("Create GitHub issue")
            onClicked: {
                Core.openPendingCrashIssue()
                root.issueRequested()
                root.dismissRequested()
            }
        }
    }
}
