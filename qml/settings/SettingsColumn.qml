import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// Body of a settings page: a readable column (max 780 px) with even spacing between groups.
ColumnLayout {
    id: root

    property int margin: MD.Token.spacing.large

    x: margin
    width: Math.min((parent ? parent.width : 800) - margin * 2, 780)
    spacing: 28
    // Air between the page title and the first group.
    y: MD.Token.spacing.medium
}
