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

    implicitHeight: row.implicitHeight + 2 * padding

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: MD.Token.spacing.small

        Rectangle {
            Layout.preferredWidth: 10
            Layout.preferredHeight: 10
            radius: 5
            color: MD.Token.color.primary

            SequentialAnimation on opacity {
                running: true
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
            source: root.coverUrl
            seed: root.title
            fallbackText: root.title.charAt(0)
            cornerRadius: MD.Token.shape.corner.medium
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Сейчас играете")
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
            text: qsTr("Остановить")
            mdState.type: MD.Enum.BtFilled
            mdState.backgroundColor: MD.Token.color.error
            mdState.textColor: MD.Token.color.on_error
            onClicked: Core.stopRunningGame()
        }
    }
}
