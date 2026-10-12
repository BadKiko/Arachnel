import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// Centered placeholder for an empty page: an icon in an expressive shape that springs in, a bold
// title, a hint and one optional action. Fades in and out with `shown` instead of popping.
Item {
    id: root

    property bool shown: true
    property string title
    // Page name kept at the top-left so an empty page still reads as the same screen.
    property string pageTitle
    property string message
    property string actionText
    property string iconName: MD.Token.icon.sports_esports
    property string shapeKind: "cookie9"
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

    DisplayTitle {
        visible: root.pageTitle.length > 0
        x: root.margin
        y: root.margin
        text: root.pageTitle
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: MD.Token.spacing.medium
        width: Math.min(root.width - root.margin * 2, 520)

        ShapeIcon {
            id: hero
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 152
            Layout.preferredHeight: 152
            Layout.bottomMargin: MD.Token.spacing.small
            iconName: root.iconName
            kind: root.shapeKind
            color: MD.Token.color.primary_container
            iconColor: MD.Token.color.on_primary_container
            iconSize: 64

            // Springs in when the page shows it.
            scale: root.shown ? 1 : 0.6
            rotation: root.shown ? 0 : -40
            Behavior on scale {
                NumberAnimation { duration: AppMotion.scaled(650); easing.type: Easing.OutBack; easing.overshoot: 1.8 }
            }
            Behavior on rotation {
                NumberAnimation { duration: AppMotion.scaled(800); easing.type: Easing.OutBack; easing.overshoot: 1.2 }
            }
        }

        MD.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: root.title
            typescale: MD.Token.typescale.headline_medium
            font.weight: Font.DemiBold
        }

        MD.Label {
            Layout.fillWidth: true
            visible: root.message.length > 0
            horizontalAlignment: Text.AlignHCenter
            text: root.message
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_large
            wrapMode: Text.WordWrap
        }

        MD.Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: MD.Token.spacing.small
            visible: root.actionText.length > 0
            text: root.actionText
            icon.name: MD.Token.icon.storefront
            mdState.type: MD.Enum.BtFilled
            mdState.size: MD.Enum.M
            onClicked: root.actionTriggered()
        }
    }
}
