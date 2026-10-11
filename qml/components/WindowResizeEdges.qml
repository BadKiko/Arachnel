import QtQuick

Item {
    id: root

    required property var window
    property int margin: 6
    // Corners are a bit larger than the edges so a diagonal resize is easy to hit.
    property int corner: 12

    // A maximized window can't be resized, so its edges shouldn't promise it.
    visible: root.window.visibility !== Window.Maximized && root.window.visibility !== Window.FullScreen

    function resize(edge) {
        if (root.window.visibility !== Window.Maximized)
            root.window.startSystemResize(edge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: margin
        hoverEnabled: true
        cursorShape: Qt.SizeVerCursor
        onPressed: resize(Qt.TopEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: margin
        hoverEnabled: true
        cursorShape: Qt.SizeVerCursor
        onPressed: resize(Qt.BottomEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: margin
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        onPressed: resize(Qt.LeftEdge)
    }

    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: margin
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        onPressed: resize(Qt.RightEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        width: corner
        height: corner
        hoverEnabled: true
        cursorShape: Qt.SizeFDiagCursor
        onPressed: resize(Qt.LeftEdge | Qt.TopEdge)
    }

    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        width: corner
        height: corner
        hoverEnabled: true
        cursorShape: Qt.SizeBDiagCursor
        onPressed: resize(Qt.RightEdge | Qt.TopEdge)
    }

    MouseArea {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: corner
        height: corner
        hoverEnabled: true
        cursorShape: Qt.SizeBDiagCursor
        onPressed: resize(Qt.LeftEdge | Qt.BottomEdge)
    }

    MouseArea {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: corner
        height: corner
        hoverEnabled: true
        cursorShape: Qt.SizeFDiagCursor
        onPressed: resize(Qt.RightEdge | Qt.BottomEdge)
    }
}
