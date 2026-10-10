import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    readonly property bool onLinux: Qt.platform.os === "linux"

    signal openSection(string sectionId)

    readonly property var sectionModel: {
        const sections = [
            {
                id: "plugins", group: 0, icon: MD.Token.icon.extension, shape: "cookie9",
                title: qsTr("Plugins"),
                subtitle: qsTr("Game sources - install one to browse and play games.")
            },
            {
                id: "sources", group: 0, icon: MD.Token.icon.playlist_add, shape: "clover4",
                title: qsTr("Hydra catalogs"),
                subtitle: qsTr("Add extra game lists from a link, e.g. a Hydra Launcher list (advanced)")
            },
            {
                id: "friends", group: 1, icon: MD.Token.icon.group, shape: "clover8",
                title: qsTr("Friends"),
                subtitle: qsTr("Friend codes and online status")
            },
            {
                id: "storage", group: 0, icon: MD.Token.icon.hard_drive, shape: "cookie6",
                title: qsTr("Storage"),
                subtitle: qsTr("Library and download folders")
            },
            {
                id: "updates", group: 0, icon: MD.Token.icon.update, shape: "sunny",
                title: qsTr("Updates"),
                subtitle: qsTr("Game and launcher updates")
            },
            {
                id: "launch", group: 2, icon: MD.Token.icon.rocket_launch, shape: "burst",
                title: qsTr("Launch"),
                subtitle: qsTr("Launch options and Proton on Linux")
            },
            {
                id: "appearance", group: 2, icon: MD.Token.icon.palette, shape: "cookie4",
                title: qsTr("Appearance"),
                subtitle: qsTr("Theme, colors, and language")
            },
            {
                id: "about", group: 2, icon: MD.Token.icon.info, shape: "circle",
                title: qsTr("About"),
                subtitle: qsTr("Version and app data")
            }
        ]
        if (root.onLinux)
            return sections
        return sections.filter(function (entry) { return entry.id !== "launch" })
    }

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.small

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            Layout.bottomMargin: MD.Token.spacing.small
            spacing: MD.Token.spacing.small

            // Sections in three groups, each drawn as one connected list.
            Repeater {
                model: [
                    { label: qsTr("Games and storage"), group: 0 },
                    { label: qsTr("People"), group: 1 },
                    { label: qsTr("App"), group: 2 }
                ]

                delegate: ColumnLayout {
                    id: groupBlock
                    required property var modelData
                    readonly property var rows: root.sectionModel.filter(e => e.group === modelData.group)

                    Layout.fillWidth: true
                    Layout.topMargin: MD.Token.spacing.small
                    spacing: 2
                    visible: rows.length > 0

                    MD.Label {
                        Layout.leftMargin: MD.Token.spacing.medium
                        Layout.bottomMargin: MD.Token.spacing.small
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
                            first: index === 0
                            last: index === groupBlock.rows.length - 1
                            title: modelData.title
                            subtitle: modelData.subtitle
                            iconName: modelData.icon
                            shapeKind: modelData.shape
                            onClicked: root.openSection(modelData.id)
                        }
                    }
                }
            }
        }
    }
}
