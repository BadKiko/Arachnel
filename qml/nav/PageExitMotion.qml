import QtQuick
import Qcm.Material as MD

// Fast exit so the incoming page isn't ghosted over the old one.
Transition {
    OpacityAnimator {
        from: 1.0
        to: 0.0
        duration: MD.Token.duration.short3
        easing: MD.Token.easing.emphasized_accelerate
    }
}
