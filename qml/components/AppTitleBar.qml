import QtQuick

import Qcm.Material as MD

Item {
    id: root

    required property var window

    implicitHeight: 32
    height: implicitHeight

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

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: function (mouse) {
            if (mouse.button === Qt.LeftButton)
                root.window.startSystemMove()
        }
        onDoubleClicked: {
            if (root.window.visibility === Window.Maximized)
                root.window.showNormal()
            else
                root.window.showMaximized()
        }
    }

    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height
        spacing: 0
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
