import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal

    function openTrust() {
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
            text: qsTr("Steam CDN + Online Fix")
            typescale: MD.Token.typescale.headline_small
        }

        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(bodyLabel.implicitHeight, 420)
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            contentWidth: width
            contentHeight: bodyLabel.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            // Markdown: MD.Label has no textFormat; use Text with typescale tokens.
            Text {
                id: bodyLabel
                width: parent.width
                wrapMode: Text.WordWrap
                textFormat: Text.MarkdownText
                color: MD.Token.color.on_surface
                font.pixelSize: MD.Token.typescale.body_medium.size
                text: Messages.steamidraTrustMarkdown
            }

            ScrollBar.vertical: MD.ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }

        MD.Button {
            Layout.alignment: Qt.AlignRight
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            text: qsTr("Got it")
            mdState.type: MD.Enum.BtFilled
            onClicked: root.close()
        }
    }
}
