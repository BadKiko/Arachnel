import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Templates as T

import Qcm.Material as MD

MD.Dialog {
    id: root

    parent: Overlay.overlay
    title: qsTr("Catalogs and plugins")
    standardButtons: T.DialogButtonBox.Close
    modal: true

    readonly property var steps: [
        {
            icon: MD.Token.icon.extension,
            step: qsTr("Step 1"),
            title: qsTr("Hydra catalog"),
            body: Messages.helpHydraCatalogBody
        },
        {
            icon: MD.Token.icon.storefront,
            step: qsTr("Step 2"),
            title: qsTr("Catalog"),
            body: Messages.helpCatalogBody
        },
        {
            icon: MD.Token.icon.sports_esports,
            step: qsTr("Step 3"),
            title: qsTr("Library"),
            body: Messages.helpLibraryBody
        }
    ]

    contentItem: ColumnLayout {
        spacing: MD.Token.spacing.medium
        width: parent ? parent.width : implicitWidth

        MD.Label {
            Layout.fillWidth: true
            text: Messages.helpCatalogIntro
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        Repeater {
            model: root.steps

            Rectangle {
                required property var modelData

                Layout.fillWidth: true
                radius: MD.Token.shape.corner.large
                color: MD.Token.color.surface_container
                implicitHeight: stepCol.implicitHeight + MD.Token.spacing.medium * 2

                ColumnLayout {
                    id: stepCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.medium
                    spacing: MD.Token.spacing.small

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.medium

                        Rectangle {
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 40
                            radius: MD.Token.shape.corner.medium
                            color: MD.Token.color.secondary_container

                            MD.Icon {
                                anchors.centerIn: parent
                                name: modelData.icon
                                size: 22
                                color: MD.Token.color.on_secondary_container
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            MD.Label {
                                Layout.fillWidth: true
                                text: modelData.step
                                color: MD.Token.color.primary
                                typescale: MD.Token.typescale.label_medium
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: modelData.title
                                typescale: MD.Token.typescale.title_small
                            }
                        }
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: modelData.body
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
