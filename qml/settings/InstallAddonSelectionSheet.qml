import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal
    property string entryId: ""
    property string entryTitle: ""

    signal confirmed(string entryId, string entryTitle, var selectedAddonIds)

    function openForEntry(id, title) {
        entryId = id
        entryTitle = title || ""
        addonModel.clear()
        const addons = Core.catalog.addonsFor(id)
        for (let i = 0; i < addons.length; ++i) {
            const addon = addons[i]
            addonModel.append({
                addonId: addon.id,
                title: addon.title,
                subtitle: (addon.kindLabel || "") + " · " + (addon.deliveryLabel || "")
                            + (addon.fileSize ? " · " + addon.fileSize : ""),
                optional: !!addon.optional,
                checked: !addon.optional,
            })
        }
        open()
    }

    function selectedAddonIds() {
        const ids = []
        for (let i = 0; i < addonModel.count; ++i) {
            if (addonModel.get(i).checked)
                ids.push(addonModel.get(i).addonId)
        }
        return ids
    }

    function setAllChecked(value) {
        for (let i = 0; i < addonModel.count; ++i)
            addonModel.setProperty(i, "checked", value)
    }

    ListModel {
        id: addonModel
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: qsTr("Add-ons")
            typescale: MD.Token.typescale.headline_medium
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: entryTitle.length
                  ? qsTr("Add-ons are available for \"%1\" — choose what to download with the game.")
                    .arg(entryTitle)
                  : qsTr("Choose add-ons to download together with the game.")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small

            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("All")
                onClicked: root.setAllChecked(true)
            }

            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("Deselect")
                onClicked: root.setAllChecked(false)
            }

            Item { Layout.fillWidth: true }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small

            Repeater {
                model: addonModel

                Rectangle {
                    id: addonRow
                    required property string addonId
                    required property string title
                    required property string subtitle
                    required property bool optional
                    required property bool checked

                    Layout.fillWidth: true
                    radius: MD.Token.shape.corner.large
                    color: addonRow.checked
                           ? MD.Util.transparent(MD.Token.color.primary, 0.08)
                           : MD.Token.color.surface_container
                    border.width: 1
                    border.color: addonRow.checked
                                ? MD.Token.color.primary
                                : MD.Token.color.outline_variant
                    implicitHeight: row.implicitHeight + MD.Token.spacing.medium * 2

                    property int rowIndex: {
                        for (let i = 0; i < addonModel.count; ++i) {
                            if (addonModel.get(i).addonId === addonRow.addonId)
                                return i
                        }
                        return -1
                    }

                    RowLayout {
                        id: row
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: MD.Token.spacing.medium
                        spacing: MD.Token.spacing.small

                        ThemedCheckBox {
                            checked: addonRow.checked
                            onToggled: function (value) {
                                if (addonRow.rowIndex >= 0)
                                    addonModel.setProperty(addonRow.rowIndex, "checked", value)
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: textCol.implicitHeight

                            ColumnLayout {
                                id: textCol
                                anchors.fill: parent
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: MD.Token.spacing.small

                                    MD.Label {
                                        Layout.fillWidth: true
                                        text: title
                                        typescale: MD.Token.typescale.title_small
                                        wrapMode: Text.WordWrap
                                    }

                                    MD.AssistChip {
                                        visible: optional
                                        text: qsTr("Optional")
                                    }
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: subtitle
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_small
                                    wrapMode: Text.WordWrap
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (addonRow.rowIndex >= 0)
                                        addonModel.setProperty(addonRow.rowIndex, "checked", !addonRow.checked)
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtText
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtFilled
                text: qsTr("Next")
                enabled: root.entryId.length > 0
                onClicked: {
                    const ids = root.selectedAddonIds()
                    root.confirmed(root.entryId, root.entryTitle, ids)
                    root.close()
                }
            }
        }
    }
}
