import QtQuick

// Appear transition for list/grid delegates (populate / add): fade + soft scale-in.
// The stagger is capped (AppMotion.staggerMaxItems), so long lists never crawl.
Transition {
    // Stagger by position for the first fill; single inserted items should not wait.
    property bool staggered: true

    SequentialAnimation {
        PauseAnimation {
            duration: staggered ? AppMotion.stagger(ViewTransition.index) : 0
        }
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: AppMotion.medium
                easing: AppMotion.standardDecelerate
            }
            NumberAnimation {
                property: "scale"
                from: 0.94
                to: 1.0
                duration: AppMotion.medium
                easing: AppMotion.emphasizedDecelerate
            }
        }
    }
}
