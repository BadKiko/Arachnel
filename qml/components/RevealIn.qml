import QtQuick

// Drop inside any Item to make it fade and settle into place. By default that happens when the page
// opens; give it a `flickable` and it waits until the item scrolls into view. `delay` staggers
// siblings; with "reduce motion" on the item simply appears.
Item {
    id: root

    property Item target: parent
    property Flickable flickable: null
    property int delay: 0
    property real rise: 0.96

    property bool _done: false

    visible: false
    width: 0
    height: 0

    function check() {
        if (_done || !target)
            return
        if (flickable) {
            const top = target.mapToItem(flickable, 0, 0).y
            if (top > flickable.height * 0.94)
                return
        }
        _done = true
        reveal.start()
    }

    Component.onCompleted: {
        if (!target || AppMotion.reduceMotion) {
            _done = true
            return
        }
        target.opacity = 0
        target.scale = root.rise
        Qt.callLater(check)
    }

    Connections {
        target: root.flickable
        enabled: !root._done && root.flickable !== null
        function onContentYChanged() { root.check() }
        function onHeightChanged() { root.check() }
    }

    SequentialAnimation {
        id: reveal

        PauseAnimation { duration: root.delay }
        ParallelAnimation {
            NumberAnimation {
                target: root.target
                property: "opacity"
                to: 1
                duration: AppMotion.long
                easing: AppMotion.emphasizedDecelerate
            }
            NumberAnimation {
                target: root.target
                property: "scale"
                to: 1
                duration: AppMotion.extraLong
                easing.type: Easing.OutBack
                easing.overshoot: 1.4
            }
        }
    }
}
