import QtQuick
import Qcm.Material as MD

// Soft enter: fade + light scale. Avoid strong OutBack overshoot - it clips the
// discovery hero under RoundClip / StackView during push/pop from game details.
Transition {
    ParallelAnimation {
        OpacityAnimator {
            from: 0.0
            to: 1.0
            duration: MD.Token.duration.medium4
            easing: MD.Token.easing.emphasized_decelerate
        }
        ScaleAnimator {
            from: 0.96
            to: 1.0
            duration: MD.Token.duration.medium4
            easing: MD.Token.easing.emphasized_decelerate
        }
    }
}
