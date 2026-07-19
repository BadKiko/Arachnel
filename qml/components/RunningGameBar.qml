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

        Rectangle {
            Layout.preferredWidth: 10
            Layout.preferredHeight: 10
            Layout.alignment: Qt.AlignVCenter
            radius: 5
            color: MD.Token.color.primary

            SequentialAnimation on opacity {
                running: root.visible
                loops: Animation.Infinite
                NumberAnimation {
                    from: 1.0
                    to: 0.35
                    duration: 900
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 0.35
                    to: 1.0
                    duration: 900
                    easing.type: Easing.InOutQuad
                }
            }
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
