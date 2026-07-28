import QtQuick
import Qcm.Material as MD

// Soft enter: fade only. Scale on the main pages stack was clipping Discover
// shelves under RoundClip after push/pop from game details.
Transition {
    OpacityAnimator {
        from: 0.0
        to: 1.0
        duration: MD.Token.duration.medium4
        easing: MD.Token.easing.emphasized_decelerate
    }
}
