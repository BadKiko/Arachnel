import QtQuick

import Qcm.Material as MD

Item {
    id: root

    required property var window

    // Read and written by the native Snap Layouts filter (src/app/snap_layouts_filter.cpp).
    objectName: "appTitleBar"
    property rect maxButtonRect: Qt.rect(0, 0, 0, 0)
    property bool maxButtonHovered: false
    property bool maxButtonPressed: false

    function toggleMaximize() {
        if (root.window.visibility === Window.Maximized)
            root.window.showNormal()
        else
            root.window.showMaximized()
    }

    function updateMaxButtonRect() {
        if (!maxButton.visible || !maxButton.width) {
            root.maxButtonRect = Qt.rect(0, 0, 0, 0)
            return
        }
        const p = maxButton.mapToItem(null, 0, 0)
        root.maxButtonRect = Qt.rect(p.x, p.y, maxButton.width, maxButton.height)
    }

    onWidthChanged: updateMaxButtonRect()
    onVisibleChanged: updateMaxButtonRect()
    Component.onCompleted: updateMaxButtonRect()

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
            id: maxButton
            iconName: root.window.visibility === Window.Maximized
                      ? MD.Token.icon.fullscreen_exit
                      : MD.Token.icon.crop_square
            // Windows answers the hit-test for this button itself (Snap Layouts), so Qt gets no
            // mouse events over it; hover / press come from the native filter instead.
            forceHovered: root.maxButtonHovered
            forcePressed: root.maxButtonPressed
            onClicked: root.toggleMaximize()
            onXChanged: root.updateMaxButtonRect()
            onWidthChanged: root.updateMaxButtonRect()
        }

        WindowChromeButton {
            iconName: MD.Token.icon.close
            danger: true
            onClicked: root.window.close()
        }
    }
}
