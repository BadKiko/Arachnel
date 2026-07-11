import QtQuick

Item {
    id: root

    required property var window
    property int margin: 5

    function resize(edge) {
        if (root.window.visibility !== Window.Maximized)
            root.window.startSystemResize(edge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: margin
        onPressed: resize(Qt.TopEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: margin
        onPressed: resize(Qt.BottomEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: margin
        onPressed: resize(Qt.LeftEdge)
    }

    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: margin
        onPressed: resize(Qt.RightEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        width: margin
        height: margin
        onPressed: resize(Qt.LeftEdge | Qt.TopEdge)
    }

    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        width: margin
        height: margin
        onPressed: resize(Qt.RightEdge | Qt.TopEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: margin
        height: margin
        onPressed: resize(Qt.LeftEdge | Qt.BottomEdge)
    }

    MouseArea {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: margin
        height: margin
        onPressed: resize(Qt.RightEdge | Qt.BottomEdge)
    }
}
