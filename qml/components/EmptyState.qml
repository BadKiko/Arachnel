import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// Centered placeholder for an empty page: web mark, title, hint and one optional action.
// Fades in and out with `shown` instead of popping.
Item {
    id: root

    property bool shown: true
    property string title
    property string message
    property string actionText
    property int margin: Appearance.pageMargin

    signal actionTriggered()

    anchors.fill: parent
    opacity: root.shown ? 1 : 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation {
            duration: AppMotion.short
            easing: AppMotion.standard
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: MD.Token.spacing.medium
        width: Math.min(root.width - root.margin * 2, 420)

        SpiderWebMark {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 160
            Layout.preferredHeight: 160
            width: 160
            height: 160
            strokeColor: MD.Token.color.primary
            strokeWidth: 2.5
            opacity: 0.35
        }

        MD.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: root.title
            typescale: MD.Token.typescale.headline_small
        }

        MD.Label {
            Layout.fillWidth: true
            visible: root.message.length > 0
            horizontalAlignment: Text.AlignHCenter
            text: root.message
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        MD.Button {
            Layout.alignment: Qt.AlignHCenter
            visible: root.actionText.length > 0
            text: root.actionText
            mdState.type: MD.Enum.BtFilled
            onClicked: root.actionTriggered()
        }
    }
}
