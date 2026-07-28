import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.Pane {
    id: root

    property string gameId: ""
    property string title: ""
    property string coverUrl: ""

    padding: MD.Token.spacing.small
    radius: MD.Token.shape.corner.large
    backgroundColor: MD.Token.color.primary_container
    clip: true

    contentItem: RowLayout {
        id: content
        spacing: MD.Token.spacing.small

        MD.CircularIndicator {
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            Layout.alignment: Qt.AlignVCenter
            indeterminate: true
            running: root.visible
            strokeWidth: 2.5
        }

        GamePoster {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 54
            Layout.alignment: Qt.AlignVCenter
            source: root.coverUrl
            seed: root.title
            fallbackText: root.title.charAt(0)
            cornerRadius: MD.Token.shape.corner.medium
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 0

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Playing now")
                color: MD.Token.color.on_primary_container
                typescale: MD.Token.typescale.label_medium
                elide: Text.ElideRight
            }

            MD.Label {
                Layout.fillWidth: true
                text: root.title
                color: MD.Token.color.on_primary_container
                typescale: MD.Token.typescale.title_small
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        MD.Button {
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Stop")
            mdState.type: MD.Enum.BtFilled
            mdState.backgroundColor: MD.Token.color.error
            mdState.textColor: MD.Token.color.on_error
            onClicked: Core.stopRunningGame()
        }
    }
}
