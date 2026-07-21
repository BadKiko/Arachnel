import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    required property var page
    required property int pageMargin

    property alias searchText: catalogSearch.searchText

    implicitHeight: catalogStickyCol.implicitHeight + MD.Token.spacing.small

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: -MD.Token.spacing.medium
        anchors.leftMargin: -pageMargin
        anchors.rightMargin: -pageMargin
        color: MD.Token.color.surface
    }

    ColumnLayout {
        id: catalogStickyCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: MD.Token.spacing.small

        MD.SearchBar {
            id: catalogSearch
            Layout.fillWidth: true
            Layout.maximumWidth: 560
            Layout.alignment: Qt.AlignLeft
            onAccepted: page.applyCatalogSearch(searchText.trim())
            onSearchTextChanged: {
                if (!searchText.length)
                    page.applyCatalogSearch("")
            }

            Connections {
                target: page
                function onSearchQueryChanged() {
                    if (catalogSearch.searchText !== page.searchQuery)
                        catalogSearch.searchText = page.searchQuery
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small
            visible: Core.catalogActiveFilterCount > 0

            MD.FilterChip {
                visible: Core.catalogTypeFilter >= 0
                text: (page.typeFilterLabels[String(Core.catalogTypeFilter)] || qsTr("Type"))
                      + " ×"
                checked: true
                onClicked: Core.catalogTypeFilter = -1
            }

            MD.FilterChip {
                visible: Core.catalogSizeFilter > 0
                text: (page.sizeFilterLabels[String(Core.catalogSizeFilter)] || qsTr("Size"))
                      + " ×"
                checked: true
                onClicked: Core.catalogSizeFilter = 0
            }

            MD.FilterChip {
                visible: Core.catalogRecencyFilter > 0
                text: (page.recencyFilterLabels[String(Core.catalogRecencyFilter)] || qsTr("Added"))
                      + " ×"
                checked: true
                onClicked: Core.catalogRecencyFilter = 0
            }

            MD.FilterChip {
                visible: Core.catalogHasAddonsFilter
                text: qsTr("Has add-ons") + " ×"
                checked: true
                onClicked: Core.catalogHasAddonsFilter = false
            }

            MD.FilterChip {
                visible: Core.catalogGenreFilter.length > 0
                text: Core.catalogGenreFilter + " ×"
                checked: true
                onClicked: Core.catalogGenreFilter = ""
            }
        }
    }
}
