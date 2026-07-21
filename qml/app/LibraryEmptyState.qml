import QtQuick
import QtQuick.Layouts
import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    required property var page

    // ── Empty library ────────────────────────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: page.libraryEmpty

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: page.pageMargin
            anchors.rightMargin: page.pageMargin
            spacing: MD.Token.spacing.large

            Item {
                id: emptyHero
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                clip: true

                layer.enabled: true
                layer.effect: MD.RoundClip {
                    corners: MD.Util.corners(page.cardRadius)
                    size: Qt.vector2d(emptyHero.width, emptyHero.height)
                }

                Rectangle {
                    anchors.fill: parent
                    color: MD.Token.color.surface_container_high
                }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: MD.Util.transparent(MD.Token.color.primary, 0.12)
                        }
                        GradientStop {
                            position: 1.0
                            color: "transparent"
                        }
                    }
                }

                SpiderWebMark {
                    width: 340
                    height: 340
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: -170
                    strokeColor: MD.Token.color.primary
                    strokeWidth: 3
                    opacity: 0.20
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: MD.Token.spacing.extra_large
                    anchors.rightMargin: MD.Token.spacing.extra_large + 120
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Nothing here yet")
                        typescale: MD.Token.typescale.headline_medium
                        wrapMode: Text.WordWrap
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 520
                        text: Messages.libraryEmptySubtitle
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.topMargin: MD.Token.spacing.small
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            text: Core.sources.enabledCount > 0
                                  ? qsTr("Open catalog")
                                  : qsTr("Install plugin")
                            icon.name: Core.sources.enabledCount > 0
                                       ? MD.Token.icon.storefront
                                       : MD.Token.icon.add
                            mdState.type: MD.Enum.BtFilled
                            onClicked: {
                                if (Core.sources.enabledCount > 0)
                                    page.openCatalog()
                                else
                                    page.addSourceRequested()
                            }
                        }

                        MD.Button {
                            visible: Core.sources.enabledCount > 0
                            text: qsTr("Settings")
                            icon.name: MD.Token.icon.settings
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: page.openSettings()
                        }

                        MD.Button {
                            visible: Core.sources.enabledCount === 0
                            text: qsTr("Catalogs and plugins")
                            mdState.type: MD.Enum.BtText
                            onClicked: sourceHelpDialog.open()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.medium

                Repeater {
                    model: [
                        {
                            icon: MD.Token.icon.extension,
                            step: qsTr("Step 1"),
                            title: qsTr("Plugin"),
                            body: Messages.libraryStep1Body
                        },
                        {
                            icon: MD.Token.icon.storefront,
                            step: qsTr("Step 2"),
                            title: qsTr("Catalog"),
                            body: Messages.libraryStep2Body
                        },
                        {
                            icon: MD.Token.icon.sports_esports,
                            step: qsTr("Step 3"),
                            title: qsTr("Library"),
                            body: Messages.libraryStep3Body
                        }
                    ]

                    Item {
                        id: tipHost
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: tipInner.implicitHeight + MD.Token.spacing.large * 2

                        Item {
                            id: tipCard
                            anchors.fill: parent
                            clip: true

                            layer.enabled: true
                            layer.effect: MD.RoundClip {
                                corners: MD.Util.corners(page.cardRadius)
                                size: Qt.vector2d(tipCard.width, tipCard.height)
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: MD.Token.color.surface_container
                            }

                            ColumnLayout {
                                id: tipInner
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: MD.Token.spacing.large
                                spacing: MD.Token.spacing.small

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: MD.Token.spacing.medium

                                    Rectangle {
                                        Layout.preferredWidth: 48
                                        Layout.preferredHeight: 48
                                        radius: MD.Token.shape.corner.medium
                                        color: MD.Token.color.secondary_container

                                        MD.Icon {
                                            anchors.centerIn: parent
                                            name: tipHost.modelData.icon
                                            size: 24
                                            color: MD.Token.color.on_secondary_container
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        MD.Label {
                                            Layout.fillWidth: true
                                            text: tipHost.modelData.step
                                            color: MD.Token.color.primary
                                            typescale: MD.Token.typescale.label_medium
                                        }

                                        MD.Label {
                                            Layout.fillWidth: true
                                            text: tipHost.modelData.title
                                            typescale: MD.Token.typescale.title_small
                                        }
                                    }
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: tipHost.modelData.body
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_medium
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    SourceHelpDialog {
        id: sourceHelpDialog
    }
}
