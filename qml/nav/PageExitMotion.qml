import QtQuick
import Qcm.Material as MD

// Fast exit so the incoming page isn't ghosted over the old one.
Transition {
    OpacityAnimator {
        from: 1.0
        to: 0.0
        duration: AppMotion.quick
        easing: AppMotion.emphasizedAccelerate
    }
}
