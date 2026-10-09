import QtQuick
import Qcm.Material as MD

// Material shared-axis (Z) enter: fade in while settling from `fromScale` to 1.
// Push brings the page in from slightly smaller, pop from slightly larger.
Transition {
    property real fromScale: 0.92

    OpacityAnimator {
        from: 0.0
        to: 1.0
        duration: AppMotion.long
        easing: AppMotion.emphasizedDecelerate
    }

    ScaleAnimator {
        from: fromScale
        to: 1.0
        duration: AppMotion.long
        easing: AppMotion.emphasizedDecelerate
    }
}
