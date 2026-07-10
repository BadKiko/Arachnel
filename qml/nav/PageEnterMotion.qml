import QtQuick
import Qcm.Material as MD

// Bounce enter: fade + scale overshoot (OutBack).
Transition {
    ParallelAnimation {
        OpacityAnimator {
            from: 0.0
            to: 1.0
            duration: MD.Token.duration.medium4
            easing: MD.Token.easing.emphasized_decelerate
        }
        ScaleAnimator {
            from: 0.90
            to: 1.0
            duration: MD.Token.duration.medium4
            easing.type: Easing.OutBack
            easing.overshoot: 1.35
        }
    }
}
