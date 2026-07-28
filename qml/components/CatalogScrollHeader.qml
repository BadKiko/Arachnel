import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property int contentWidth
    required property bool hasSelection
    required property bool listViewMode
    property real collapseProgress: 0

    signal filterRequested()
    signal viewModeChangeRequested(int mode)
    signal refreshRequested()

    width: contentWidth
    height: col.implicitHeight
    // Hand controls off to the compact bar as it lands.
    opacity: 1 - root.collapseProgress * 0.85

    ColumnLayout {
        id: col
        width: root.contentWidth
        spacing: MD.Token.spacing.small

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: MD.Token.spacing.small
            Layout.rightMargin: MD.Token.spacing.small
            spacing: MD.Token.spacing.small

            Item {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 20
                visible: Core.catalogLoading

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: MD.Util.transparent(MD.Token.color.primary, 0.12)
                }

                MD.LinearIndicator {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    implicitHeight: 4
                    strokeWidth: implicitHeight
                    indeterminate: true
                    running: Core.catalogLoading && root.visible
                    color: MD.Token.color.primary
                    trackColor: MD.Util.transparent(color, 0.2)
                }
            }

            MD.Label {
                Layout.fillWidth: true
                text: Core.catalogLoading && Core.catalog.count === 0
                    ? qsTr("Loading catalog…")
                    : qsTr("Found: %1").arg(Core.catalog.count)
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.label_large
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Item {
                Layout.preferredWidth: filterBtn.implicitWidth
                Layout.preferredHeight: filterBtn.implicitHeight

                MD.IconButton {
                    id: filterBtn
                    anchors.centerIn: parent
                    mdState.type: Core.catalogActiveFilterCount > 0
                                  ? MD.Enum.IBtFilledTonal
                                  : MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.filter_list
                    onClicked: root.filterRequested()
                }

                Rectangle {
                    visible: Core.catalogActiveFilterCount > 0
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: 2
                    anchors.topMargin: 2
                    width: Math.max(18, badgeLabel.implicitWidth + 6)
                    height: 18
                    radius: 9
                    color: MD.Token.color.error
                    z: 2

                    MD.Label {
                        id: badgeLabel
                        anchors.centerIn: parent
                        text: Core.catalogActiveFilterCount
                        color: MD.Token.color.on_error
                        typescale: MD.Token.typescale.label_small
                    }
                }
            }

            MD.IconButton {
                mdState.type: root.listViewMode ? MD.Enum.IBtStandard : MD.Enum.IBtFilledTonal
                icon.name: MD.Token.icon.grid_view
                onClicked: root.viewModeChangeRequested(0)
            }

            MD.IconButton {
                mdState.type: root.listViewMode ? MD.Enum.IBtFilledTonal : MD.Enum.IBtStandard
                icon.name: MD.Token.icon.view_list
                onClicked: root.viewModeChangeRequested(1)
            }

            MD.IconButton {
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.refresh
                enabled: !Core.catalogLoading && root.hasSelection
                onClicked: root.refreshRequested()
            }
        }

        MD.Label {
            Layout.fillWidth: true
            visible: Core.catalogStatus.length > 0
            text: Core.catalogStatus
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_small
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Item {
            Layout.preferredHeight: MD.Token.spacing.extra_small
        }
    }
}
