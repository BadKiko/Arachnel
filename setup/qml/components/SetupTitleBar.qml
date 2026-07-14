import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

Item {
    id: root

    required property var window

    implicitHeight: 32

    Rectangle {
        anchors.fill: parent
        color: MD.Token.color.surface_container

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: MD.Token.color.outline_variant
            opacity: 0.25
        }
    }

    RowLayout {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: MD.Token.spacing.medium
        spacing: MD.Token.spacing.small

        Image {
            source: "qrc:/icons/32.png"
            width: 18
            height: 18
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        MD.Label {
            text: qsTr("Arachnel Setup")
            typescale: MD.Token.typescale.label_large
            color: MD.Token.color.on_surface
        }
    }

    MouseArea {
        anchors.fill: parent
        anchors.rightMargin: 120
        acceptedButtons: Qt.LeftButton
        onPressed: function (mouse) {
            if (mouse.button === Qt.LeftButton)
                root.window.startSystemMove()
        }
    }

    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height
        z: 1

        WindowChromeButton {
            iconName: MD.Token.icon.minimize
            onClicked: root.window.showMinimized()
        }

        WindowChromeButton {
            iconName: root.window.visibility === Window.Maximized
                      ? MD.Token.icon.fullscreen_exit
                      : MD.Token.icon.crop_square
            onClicked: {
                if (root.window.visibility === Window.Maximized)
                    root.window.showNormal()
                else
                    root.window.showMaximized()
            }
        }

        WindowChromeButton {
            iconName: MD.Token.icon.close
            danger: true
            onClicked: root.window.close()
        }
    }
}
