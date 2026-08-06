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
    property var offers: []
    property var _pendingChoice: null

    signal sourceChosen(string entryId, string offerEntryId, string sourceId, string title)

    function openForEntry(id, title) {
        entryId = id || ""
        entryTitle = title || ""
        _pendingChoice = null
        offers = entryId.length ? Core.installOffersForEntry(entryId) : []
        open()
    }

    function chooseSource(offerEntryId, sourceId) {
        _pendingChoice = {
            entryId: root.entryId,
            offerEntryId: offerEntryId || "",
            sourceId: sourceId || "",
            title: root.entryTitle
        }
        close()
    }

    onClosed: {
        const pending = _pendingChoice
        _pendingChoice = null
        if (!pending)
            return
        root.sourceChosen(pending.entryId, pending.offerEntryId, pending.sourceId, pending.title)
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: qsTr("Choose download source")
            typescale: MD.Token.typescale.headline_medium
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: entryTitle.length ? entryTitle : qsTr("Select which catalog to download from")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small

            Repeater {
                model: root.offers

                Rectangle {
                    required property var modelData

                    Layout.fillWidth: true
                    radius: MD.Token.shape.corner.large
                    color: MD.Token.color.surface_container
                    border.width: 1
                    border.color: MD.Token.color.outline_variant
                    implicitHeight: row.implicitHeight + MD.Token.spacing.medium * 2

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.chooseSource(modelData.entryId || "",
                                                     modelData.sourceId || "")
                    }

                    RowLayout {
                        id: row
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: MD.Token.spacing.medium
                        spacing: MD.Token.spacing.small

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            MD.Label {
                                Layout.fillWidth: true
                                text: modelData.sourceName || modelData.sourceId || qsTr("Unknown source")
                                typescale: MD.Token.typescale.title_small
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                visible: !!(modelData.sizeLabel && modelData.sizeLabel.length)
                                text: modelData.sizeLabel || ""
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.body_small
                            }
                        }

                        MD.Icon {
                            name: MD.Token.icon.chevron_right
                            size: 22
                            color: MD.Token.color.on_surface_variant
                        }
                    }
                }
            }
        }

        MD.Button {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            mdState.type: MD.Enum.BtText
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }
}
