import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property int countsRevision: 0
    property int sourcesRev: 0

    signal addSourceRequested()
    signal editSourceRequested(string pluginId, string name, string catalogUrl,
                               string description, bool sourceEnabled)

    readonly property var hydraCatalogs: {
        root.sourcesRev
        return Core.sources.manualCatalogs()
    }

    function formatGameCount(sourceId) {
        root.countsRevision
        const count = Core.catalogEntryCount(sourceId)
        if (count < 0)
            return qsTr("Games: …")
        return qsTr("Games: %1").arg(count)
    }

    function catalogSupport(row) {
        const hasUrl = !!(row.catalogUrl && row.catalogUrl.length)
        if (!hasUrl)
            return qsTr("No URL - catalog will not load")
        if (!row.sourceEnabled)
            return qsTr("Off")
        return root.formatGameCount(row.pluginId)
    }

    Component.onCompleted: Core.prefetchCatalogCounts()

    Connections {
        target: Core
        function onCatalogCountsChanged() {
            root.countsRevision++
        }
    }

    Connections {
        target: Core.sources
        function onSourcesChanged() {
            root.sourcesRev++
            Core.prefetchCatalogCounts()
        }
    }

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: Messages.settingsSourcesConnectHint
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        MD.Button {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            mdState.type: MD.Enum.BtFilled
            text: qsTr("Add catalog")
            icon.name: MD.Token.icon.add
            onClicked: root.addSourceRequested()
        }

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.hydraCatalogs.length === 0
            implicitHeight: emptyCol.implicitHeight + MD.Token.spacing.large * 2
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: emptyCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: MD.Token.spacing.large
                spacing: MD.Token.spacing.small

                MD.ElevationRectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: MD.Token.shape.corner.full
                    color: MD.Token.color.secondary_container
                    elevation: MD.Token.elevation.level0

                    MD.Icon {
                        anchors.centerIn: parent
                        name: MD.Token.icon.link
                        size: 24
                        color: MD.Token.color.on_secondary_container
                    }
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("No catalogs yet")
                    typescale: MD.Token.typescale.title_small
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: Messages.settingsSourcesAddHint
                    wrapMode: Text.WordWrap
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }
            }
        }

        MD.ElevationRectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: root.hydraCatalogs.length > 0
            implicitHeight: catalogCol.implicitHeight + MD.Token.spacing.small * 2
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            elevation: MD.Token.elevation.level0

            ColumnLayout {
                id: catalogCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.small
                spacing: 0

                Repeater {
                    model: root.hydraCatalogs

                    ColumnLayout {
                        id: catalogRow
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        spacing: 0

                        readonly property bool enabledOn: !!modelData.sourceEnabled
                        readonly property bool hasUrl: !!(modelData.catalogUrl
                                                          && modelData.catalogUrl.length)

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: MD.Token.spacing.small
                            Layout.rightMargin: MD.Token.spacing.extra_small
                            Layout.topMargin: MD.Token.spacing.small
                            Layout.bottomMargin: MD.Token.spacing.small
                            spacing: MD.Token.spacing.medium

                            MD.ElevationRectangle {
                                Layout.preferredWidth: 40
                                Layout.preferredHeight: 40
                                Layout.alignment: Qt.AlignVCenter
                                radius: MD.Token.shape.corner.full
                                color: MD.Token.color.surface_container_highest
                                elevation: MD.Token.elevation.level0

                                MD.Icon {
                                    anchors.centerIn: parent
                                    name: MD.Token.icon.link
                                    size: 20
                                    color: MD.Token.color.on_surface_variant
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.extra_small

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: catalogRow.modelData.name
                                    typescale: MD.Token.typescale.body_large
                                    elide: Text.ElideRight
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: root.catalogSupport(catalogRow.modelData)
                                    color: catalogRow.hasUrl ? MD.Token.color.on_surface_variant
                                                             : MD.Token.color.error
                                    typescale: MD.Token.typescale.body_small
                                    elide: Text.ElideRight
                                }
                            }

                            MD.Switch {
                                checked: catalogRow.enabledOn
                                onToggled: Core.sources.setSourceEnabled(
                                               catalogRow.modelData.pluginId, checked)
                            }

                            MD.IconButton {
                                id: moreButton
                                mdState.type: MD.Enum.IBtStandard
                                icon.name: MD.Token.icon.more_vert
                                onClicked: catalogMenu.open()

                                MD.Menu {
                                    id: catalogMenu
                                    y: parent.height
                                    autoClose: true

                                    MD.MenuItem {
                                        text: qsTr("Edit")
                                        icon.name: MD.Token.icon.edit
                                        onTriggered: root.editSourceRequested(
                                                         catalogRow.modelData.pluginId,
                                                         catalogRow.modelData.name,
                                                         catalogRow.modelData.catalogUrl,
                                                         catalogRow.modelData.description,
                                                         catalogRow.enabledOn)
                                    }

                                    MD.MenuItem {
                                        enabled: catalogRow.hasUrl
                                        text: qsTr("Open URL")
                                        icon.name: MD.Token.icon.open_in_new
                                        onTriggered: Core.openExternalUrl(
                                                         catalogRow.modelData.catalogUrl)
                                    }

                                    MD.MenuItem {
                                        text: qsTr("Delete")
                                        icon.name: MD.Token.icon.delete
                                        onTriggered: Core.sources.removeSource(
                                                         catalogRow.modelData.pluginId)
                                    }
                                }
                            }
                        }

                        MD.Divider {
                            Layout.fillWidth: true
                            visible: catalogRow.index < root.hydraCatalogs.length - 1
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: MD.Token.spacing.medium
        }
    }
}
