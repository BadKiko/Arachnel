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
    property var selectedAddonIds: []
    property bool fromAddonPicker: false
    property string selectedLibraryId: Core.settings.storageLibraries.defaultLibraryId
    property var _pendingBack: null
    property var installEntry: function (entryId, libraryId, addonIds) {
        Core.installCatalogEntry(entryId, libraryId, addonIds)
    }

    signal backToAddons(string entryId, string entryTitle, var selectedAddonIds)

    function parseSizeLabelBytes(label) {
        if (!label || !label.length)
            return 0
        const m = /^(\d+(?:\.\d+)?)\s*(B|KB|MB|GB|TB)/i.exec(String(label).trim())
        if (!m)
            return 0
        let v = parseFloat(m[1])
        const unit = m[2].toUpperCase()
        if (unit === "KB")
            v *= 1024
        else if (unit === "MB")
            v *= 1024 * 1024
        else if (unit === "GB")
            v *= 1024 * 1024 * 1024
        else if (unit === "TB")
            v *= 1024 * 1024 * 1024 * 1024
        return v
    }

    function formatSizeLabelBytes(bytes) {
        if (!(bytes > 0))
            return ""
        const units = ["B", "KB", "MB", "GB", "TB"]
        let value = bytes
        let unit = 0
        while (value >= 1024 && unit < units.length - 1) {
            value /= 1024
            unit++
        }
        const text = unit === 0 ? String(Math.round(value)) : value.toFixed(1)
        return text + " " + units[unit]
    }

    readonly property var selectedAddonRows: {
        const ids = root.selectedAddonIds || []
        if (!ids.length || !root.entryId.length)
            return []
        const want = {}
        for (let i = 0; i < ids.length; ++i)
            want[String(ids[i])] = true
        const addons = Core.catalog.addonsFor(root.entryId)
        const rows = []
        for (let i = 0; i < addons.length; ++i) {
            const addon = addons[i]
            if (!addon || !want[String(addon.id)])
                continue
            rows.push({
                id: String(addon.id),
                title: addon.title || String(addon.id),
                fileSize: addon.fileSize || ""
            })
        }
        return rows
    }

    readonly property real baseBytes: {
        if (!root.entryId.length)
            return 0
        const details = Core.entryDetails(root.entryId)
        return root.parseSizeLabelBytes(details.sizeLabel || "")
    }

    readonly property real dlcBytes: {
        const rows = root.selectedAddonRows
        let sum = 0
        for (let i = 0; i < rows.length; ++i)
            sum += root.parseSizeLabelBytes(rows[i].fileSize || "")
        return sum
    }

    readonly property string sizeSummary: {
        const base = root.baseBytes
        const dlc = root.dlcBytes
        const baseLabel = root.formatSizeLabelBytes(base)
        const dlcLabel = root.formatSizeLabelBytes(dlc)
        const totalLabel = root.formatSizeLabelBytes(base + dlc)
        if (root.selectedAddonRows.length <= 0)
            return baseLabel
        if (baseLabel.length && dlcLabel.length && totalLabel.length)
            return qsTr("About %1 on disk - game %2 + %3 DLC.").arg(totalLabel).arg(baseLabel).arg(dlcLabel)
        if (totalLabel.length)
            return qsTr("About %1 on disk with DLC.").arg(totalLabel)
        return qsTr("DLC adds to install size")
    }

    function openForEntry(id, title, addonIds, cameFromAddonPicker) {
        entryId = id
        entryTitle = title || ""
        selectedAddonIds = addonIds || []
        fromAddonPicker = !!cameFromAddonPicker
        _pendingBack = null
        selectedLibraryId = Core.settings.storageLibraries.defaultLibraryId
        open()
    }

    function goBack() {
        if (!root.fromAddonPicker) {
            root.close()
            return
        }
        _pendingBack = {
            entryId: root.entryId,
            entryTitle: root.entryTitle,
            ids: root.selectedAddonIds
        }
        close()
    }

    onClosed: {
        const pending = _pendingBack
        _pendingBack = null
        if (!pending)
            return
        root.backToAddons(pending.entryId, pending.entryTitle, pending.ids)
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: qsTr("Install")
            typescale: MD.Token.typescale.headline_medium
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: entryTitle.length ? entryTitle : qsTr("Choose a drive for installation")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: root.selectedAddonRows.length > 0
            spacing: MD.Token.spacing.extra_small

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("%1 DLC will be downloaded and enabled").arg(root.selectedAddonRows.length)
                typescale: MD.Token.typescale.label_large
                color: MD.Token.color.primary
                wrapMode: Text.WordWrap
            }

            MD.Label {
                Layout.fillWidth: true
                text: {
                    const names = []
                    const rows = root.selectedAddonRows
                    const limit = Math.min(rows.length, 4)
                    for (let i = 0; i < limit; ++i)
                        names.push(rows[i].title)
                    let line = names.join(", ")
                    if (rows.length > limit)
                        line += qsTr(" +%1 more").arg(rows.length - limit)
                    return line
                }
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_small
                wrapMode: Text.WordWrap
            }

            MD.Label {
                Layout.fillWidth: true
                visible: root.sizeSummary.length > 0
                text: root.sizeSummary
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_small
                wrapMode: Text.WordWrap
            }
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: qsTr("Install to:")
            typescale: MD.Token.typescale.label_large
            color: MD.Token.color.primary
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small

            Repeater {
                model: Core.settings.storageLibraries

                Rectangle {
                    required property string libraryId
                    required property string label
                    required property string path
                    required property bool isDefault

                    Layout.fillWidth: true
                    radius: MD.Token.shape.corner.large
                    color: root.selectedLibraryId === libraryId
                           ? MD.Token.color.secondary_container
                           : MD.Token.color.surface_container
                    border.width: 1
                    border.color: root.selectedLibraryId === libraryId
                                  ? MD.Token.color.primary
                                  : MD.Token.color.outline_variant
                    implicitHeight: row.implicitHeight + MD.Token.spacing.medium * 2

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.selectedLibraryId = libraryId
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

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.extra_small

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: label
                                    typescale: MD.Token.typescale.title_small
                                }

                                MD.Icon {
                                    visible: isDefault
                                    name: MD.Token.icon.star
                                    size: 18
                                    color: MD.Token.color.primary
                                }
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: path
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.body_small
                                elide: Text.ElideMiddle
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
                text: root.fromAddonPicker ? qsTr("Back") : qsTr("Cancel")
                onClicked: root.goBack()
            }

            MD.Button {
                Layout.fillWidth: true
                mdState.type: MD.Enum.BtFilled
                text: qsTr("Install")
                enabled: root.entryId.length > 0 && root.selectedLibraryId.length > 0
                onClicked: {
                    root.installEntry(root.entryId, root.selectedLibraryId,
                                      root.selectedAddonIds)
                    root.close()
                }
            }
        }
    }
}
