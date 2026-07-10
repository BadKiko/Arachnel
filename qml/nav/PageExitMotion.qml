import QtQuick
import Qcm.Material as MD

// Fast exit so the incoming bounce page isn't ghosted over the old one.
Transition {
    ParallelAnimation {
        OpacityAnimator {
            from: 1.0
            to: 0.0
            duration: MD.Token.duration.short3
            easing: MD.Token.easing.emphasized_accelerate
        }
        ScaleAnimator {
            from: 1.0
            to: 0.96
            duration: MD.Token.duration.short3
            easing: MD.Token.easing.emphasized_accelerate
        }
    }
}
