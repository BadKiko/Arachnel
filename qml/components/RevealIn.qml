import QtQuick

// Drop inside any Item to make it fade and settle into place when the page opens. `delay` staggers
// siblings; with "reduce motion" on the item simply appears.
Item {
    id: root

    property Item target: parent
    property int delay: 0
    property real rise: 0.96

    visible: false
    width: 0
    height: 0

    Component.onCompleted: {
        if (!target || AppMotion.reduceMotion)
            return
        target.opacity = 0
        target.scale = root.rise
        reveal.start()
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
                easing: AppMotion.emphasizedDecelerate
            }
        }
    }
}
