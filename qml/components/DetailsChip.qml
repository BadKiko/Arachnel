import QtQuick

import Qcm.Material as MD

// A chip that stays readable on top of the blurred cover backdrop: the plain outlined chip has no
// fill, so over the backdrop it melted into the picture.
MD.AssistChip {
    elevated: true
    // Sentence case: Capitalize turned "Доступно обновление" into "Доступно Обновление".
    font.capitalization: Font.MixedCase
    mdState.backgroundColor: MD.Token.color.surface_container_highest
    mdState.outlineColor: MD.Token.color.outline_variant
}
