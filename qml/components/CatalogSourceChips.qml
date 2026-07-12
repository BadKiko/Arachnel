import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Flow {
    id: root

    property int countsRevision: 0

    width: parent ? parent.width : implicitWidth
    spacing: MD.Token.spacing.small

    Connections {
        target: Core
        function onCatalogCountsChanged() {
            root.countsRevision++
        }
    }

    function chipLabel(sourceId, sourceName) {
        root.countsRevision
        const count = Core.catalogEntryCount(sourceId)
        if (count < 0)
            return sourceName
        return qsTr("%1 · %2").arg(sourceName).arg(count)
    }

    Repeater {
        model: Core.sources

        Item {
            required property string pluginId
            required property string name
            required property bool sourceEnabled

            visible: sourceEnabled
            width: chip.implicitWidth
            height: chip.implicitHeight

            readonly property bool selected:
                    Core.activeCatalogSourceIds.indexOf(pluginId) >= 0

            MD.FilterChip {
                id: chip
                anchors.fill: parent
                text: root.chipLabel(pluginId, name)
                checkable: false
                checked: parent.selected
                elevated: !parent.selected
            }

            MouseArea {
                anchors.fill: parent
                z: 1
                cursorShape: Qt.PointingHandCursor
                onClicked: Core.toggleCatalogSource(pluginId)
            }
        }
    }
}
