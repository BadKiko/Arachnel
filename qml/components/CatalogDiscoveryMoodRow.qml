import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var page
    spacing: MD.Token.spacing.extra_small

    readonly property var moods: [
        { id: "", label: qsTr("For you"), icon: MD.Token.icon.auto_awesome },
        { id: "friends", label: qsTr("With friends"), icon: MD.Token.icon.groups },
        { id: "new", label: qsTr("Something new"), icon: MD.Token.icon.fiber_new }
    ]

    MD.Label {
        Layout.fillWidth: true
        text: qsTr("Browse by mood")
        typescale: MD.Token.typescale.label_large
        color: MD.Token.color.on_surface_variant
    }

    Flow {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.small

        Repeater {
            model: root.moods

            MD.FilterChip {
                required property var modelData
                text: modelData.label
                checkable: false
                checked: (Core.catalogDiscovery.moodId || "") === modelData.id
                elevated: (Core.catalogDiscovery.moodId || "") !== modelData.id
                onClicked: Core.catalogDiscovery.moodId = modelData.id
            }
        }
    }
}
