import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

// v2 settings: a full page with two panes. Sections on the left as grouped expressive lists, the
// chosen section on the right. Nested pages (catalog form, plugin store) push inside the right
// pane with a back arrow.
Item {
    id: root

    readonly property bool onLinux: Qt.platform.os === "linux"
    readonly property int pageMargin: Appearance.pageMargin

    readonly property var sections: {
        const all = [
            { id: "plugins", group: 0, title: qsTr("Plugins"), icon: MD.Token.icon.extension, shape: "cookie9" },
            { id: "sources", group: 0, title: qsTr("Hydra catalogs"), icon: MD.Token.icon.playlist_add, shape: "clover4" },
            { id: "storage", group: 0, title: qsTr("Storage"), icon: MD.Token.icon.hard_drive, shape: "cookie6" },
            { id: "updates", group: 0, title: qsTr("Updates"), icon: MD.Token.icon.update, shape: "sunny" },
            { id: "friends", group: 1, title: qsTr("Friends"), icon: MD.Token.icon.group, shape: "clover8" },
            { id: "launch", group: 2, title: qsTr("Launch"), icon: MD.Token.icon.rocket_launch, shape: "burst" },
            { id: "appearance", group: 2, title: qsTr("Appearance"), icon: MD.Token.icon.palette, shape: "cookie4" },
            { id: "about", group: 2, title: qsTr("About"), icon: MD.Token.icon.info, shape: "circle" }
        ]
        return root.onLinux ? all : all.filter(e => e.id !== "launch")
    }

    function open(sectionId, createSource) {
        const id = sectionId && sectionId.length ? sectionId : (page.currentSection || "plugins")
        page.showSection(id, !!createSource)
    }

    Component.onCompleted: {
        if (!page.currentSection.length)
            page.showSection("plugins", false)
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: root.pageMargin
        spacing: MD.Token.spacing.large

        // ---------- Sections ----------
        Flickable {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: navCol.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: navCol
                width: parent.width
                spacing: MD.Token.spacing.small

                DisplayTitle {
                    Layout.bottomMargin: MD.Token.spacing.medium
                    text: qsTr("Settings")
                }

                Repeater {
                    model: [
                        { label: qsTr("Games and storage"), group: 0 },
                        { label: qsTr("People"), group: 1 },
                        { label: qsTr("App"), group: 2 }
                    ]

                    delegate: ColumnLayout {
                        id: groupBlock
                        required property var modelData
                        readonly property var rows: root.sections.filter(e => e.group === modelData.group)

                        Layout.fillWidth: true
                        Layout.bottomMargin: MD.Token.spacing.small
                        spacing: 2
                        visible: rows.length > 0

                        MD.Label {
                            Layout.leftMargin: MD.Token.spacing.medium
                            Layout.bottomMargin: MD.Token.spacing.extra_small
                            text: groupBlock.modelData.label
                            typescale: MD.Token.typescale.label_large
                            color: MD.Token.color.primary
                        }

                        Repeater {
                            model: groupBlock.rows

                            delegate: ExpressiveListItem {
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                implicitHeight: 60
                                first: index === 0
                                last: index === groupBlock.rows.length - 1
                                title: modelData.title
                                iconName: modelData.icon
                                shapeKind: modelData.shape
                                showChevron: false
                                selected: page.currentSection === modelData.id
                                onClicked: root.open(modelData.id, false)
                            }
                        }
                    }
                }
            }
        }

        // ---------- The section ----------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 28
            color: MD.Token.color.surface_container_low
            clip: true

            SettingsPage {
                id: page
                anchors.fill: parent
                anchors.topMargin: MD.Token.spacing.small
                embedded: true
                closeSheet: function () {}
            }
        }
    }
}
