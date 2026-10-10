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
    contentHeight: body.implicitHeight + MD.Token.spacing.extra_large
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    SettingsColumn {
        id: body
        margin: root.contentMargin

        SettingsGroup {
            title: qsTr("Catalogs")
            description: Messages.settingsSourcesConnectHint

            SettingsRow {
                visible: root.hydraCatalogs.length === 0
                title: qsTr("No catalogs yet")
                subtitle: Messages.settingsSourcesAddHint
                iconName: MD.Token.icon.link
            }

            Repeater {
                model: root.hydraCatalogs

                delegate: SettingsRow {
                    id: catalogRow
                    required property var modelData
                    readonly property bool enabledOn: !!modelData.sourceEnabled
                    readonly property bool hasUrl: !!(modelData.catalogUrl && modelData.catalogUrl.length)

                    title: modelData.name
                    subtitle: root.catalogSupport(modelData)
                    iconName: MD.Token.icon.link

                    MD.Switch {
                        checked: catalogRow.enabledOn
                        onToggled: Core.sources.setSourceEnabled(catalogRow.modelData.pluginId, checked)
                    }

                    MD.IconButton {
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
                                onTriggered: Core.openExternalUrl(catalogRow.modelData.catalogUrl)
                            }

                            MD.MenuItem {
                                text: qsTr("Delete")
                                icon.name: MD.Token.icon.delete
                                onTriggered: Core.sources.removeSource(catalogRow.modelData.pluginId)
                            }
                        }
                    }
                }
            }

            SettingsRow {
                title: qsTr("Add catalog")
                iconName: MD.Token.icon.add
                clickable: true
                showChevron: true
                onClicked: root.addSourceRequested()
            }
        }
    }
}
