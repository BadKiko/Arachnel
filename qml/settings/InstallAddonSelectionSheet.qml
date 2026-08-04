import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Window

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal
    // Same as SettingsSheet: wheel/list flick must not drag-dismiss the modal.
    dismissOnDragDown: false
    property string entryId: ""
    property string entryTitle: ""
    property bool waitingAddons: false

    signal confirmed(string entryId, string entryTitle, var selectedAddonIds)

    function shotList(raw) {
        if (raw === undefined || raw === null)
            return []
        if (Array.isArray(raw))
            return raw.filter(function (u) { return !!u && String(u).length > 0 }).map(String)
        const len = raw.length !== undefined ? raw.length : 0
        const out = []
        for (let i = 0; i < len; ++i) {
            const u = String(raw[i] || "")
            if (u.length)
                out.push(u)
        }
        return out
    }

    function populateFromCatalog() {
        addonModel.clear()
        const addons = Core.catalog.addonsFor(entryId)
        for (let i = 0; i < addons.length; ++i) {
            const addon = addons[i]
            const available = addon.contentAvailable !== false
            const shots = root.shotList(addon.screenshotUrls)
            let cover = String(addon.coverUrl || "")
            if (!cover.length && String(addon.id || "").indexOf("steam-") === 0) {
                const appId = String(addon.id).slice(6)
                if (/^\d+$/.test(appId))
                    cover = "https://cdn.akamai.steamstatic.com/steam/apps/" + appId + "/header.jpg"
            }
            addonModel.append({
                addonId: addon.id,
                title: addon.title,
                subtitle: available ? (addon.fileSize || "") : qsTr("Not on source"),
                optional: !!addon.optional,
                checked: false,
                contentAvailable: available,
                coverUrl: cover,
                screenshotUrls: shots,
            })
        }
    }

    function openForEntry(id, title) {
        entryId = id
        entryTitle = title || ""
        addonModel.clear()
        waitingAddons = true
        const ready = Core.ensureCatalogAddons ? Core.ensureCatalogAddons(id) : true
        if (ready) {
            waitingAddons = false
            populateFromCatalog()
            if (addonModel.count === 0) {
                root.confirmed(root.entryId, root.entryTitle, [])
                return
            }
            open()
            return
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

    Connections {
        target: Core
        function onCatalogAddonsReady(id) {
            if (!root.waitingAddons || id !== root.entryId)
                return
            root.waitingAddons = false
            root.populateFromCatalog()
            if (addonModel.count === 0) {
                if (root.opened)
                    root.close()
                root.confirmed(root.entryId, root.entryTitle, [])
            }
        }
    }

    ListModel {
        id: addonModel
    }

    ColumnLayout {
        id: sheetBody
        width: root.sheetWidth
        spacing: MD.Token.spacing.medium
        // Cap to the overlay so BottomSheet Flickable2 has no scroll range to steal.
        readonly property real availableHeight: {
            const w = Window.window
            if (!w)
                return 640
            return Math.max(280, w.height - root.topMargin - 48)
        }
        height: Math.min(implicitHeight, availableHeight)

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: qsTr("DLC")
            typescale: MD.Token.typescale.headline_medium
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            text: root.waitingAddons
                  ? qsTr("Loading DLC from Steam…")
                  : (entryTitle.length
                     ? qsTr("Pick DLC to install with \"%1\".").arg(entryTitle)
                     : qsTr("Pick DLC to install with the game."))
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: !root.waitingAddons && addonModel.count > 0
            spacing: MD.Token.spacing.small

            MD.Button {
                text: qsTr("All")
                mdState.type: MD.Enum.BtText
                onClicked: root.setAllChecked(true)
            }
            MD.Button {
                text: qsTr("Deselect")
                mdState.type: MD.Enum.BtText
                onClicked: root.setAllChecked(false)
            }

            Item { Layout.fillWidth: true }

            MD.Label {
                visible: addonModel.count > 0
                text: qsTr("%1 DLC").arg(addonModel.count)
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_small
            }
        }

        MD.BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: root.waitingAddons
            running: root.waitingAddons
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: !root.waitingAddons && addonModel.count === 0
            text: qsTr("No Steam DLC found for this game.")
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
        }

        ListView {
            id: addonList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.minimumHeight: root.waitingAddons ? 0 : 160
            Layout.preferredHeight: root.waitingAddons
                                    ? 0
                                    : Math.min(400, Math.max(160, addonModel.count * 118))
            visible: !root.waitingAddons && addonModel.count > 0
            clip: true
            spacing: MD.Token.spacing.small
            boundsBehavior: Flickable.StopAtBounds
            interactive: !vScroll.pressed
            reuseItems: true
            cacheBuffer: height * 2
            model: addonModel
            readonly property real scrollGutter: contentHeight > height
                                                ? MD.Token.spacing.medium + 4
                                                : 0

            ScrollBar.vertical: MD.ScrollBar {
                id: vScroll
                policy: ScrollBar.AsNeeded
                interactive: true
            }

            delegate: Rectangle {
                id: addonRow
                required property int index
                required property string addonId
                required property string title
                required property string subtitle
                required property bool optional
                required property bool checked
                required property string coverUrl
                required property var screenshotUrls

                readonly property var shots: {
                    const raw = addonRow.screenshotUrls
                    if (raw === undefined || raw === null)
                        return []
                    if (Array.isArray(raw))
                        return raw
                    const len = raw.length !== undefined ? raw.length : 0
                    const out = []
                    for (let i = 0; i < len; ++i) {
                        const u = String(raw[i] || "")
                        if (u.length)
                            out.push(u)
                    }
                    return out
                }

                width: Math.max(0, addonList.width - addonList.scrollGutter)
                radius: MD.Token.shape.corner.large
                color: addonRow.checked
                       ? MD.Util.transparent(MD.Token.color.primary, 0.08)
                       : MD.Token.color.surface_container
                border.width: 1
                border.color: addonRow.checked
                            ? MD.Token.color.primary
                            : MD.Token.color.outline_variant
                implicitHeight: col.implicitHeight + MD.Token.spacing.medium * 2
                height: implicitHeight

                ColumnLayout {
                    id: col
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.medium
                    spacing: MD.Token.spacing.small

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.CheckBox {
                            checked: addonRow.checked
                            onToggled: addonModel.setProperty(addonRow.index, "checked", checked)
                        }

                        Rectangle {
                            Layout.preferredWidth: 96
                            Layout.preferredHeight: 54
                            radius: MD.Token.shape.corner.small
                            color: MD.Token.color.surface_container_highest
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: addonRow.coverUrl
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
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
                                visible: subtitle.length > 0
                                text: subtitle
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.body_small
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Row {
                        Layout.fillWidth: true
                        Layout.preferredHeight: addonRow.shots.length > 0 ? 72 : 0
                        visible: addonRow.shots.length > 0
                        spacing: MD.Token.spacing.extra_small
                        clip: true

                        Repeater {
                            model: addonRow.shots

                            Rectangle {
                                required property string modelData
                                width: 128
                                height: 72
                                radius: MD.Token.shape.corner.small
                                color: MD.Token.color.surface_container_highest
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    source: modelData
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: true
                                }
                            }
                        }
                    }
                }

                // TapHandler doesn't steal the ListView flick (MouseArea did).
                TapHandler {
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: addonModel.setProperty(addonRow.index, "checked", !addonRow.checked)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.large
            spacing: MD.Token.spacing.small

            Item { Layout.fillWidth: true }

            MD.Button {
                text: qsTr("Cancel")
                mdState.type: MD.Enum.BtText
                onClicked: root.close()
            }

            MD.Button {
                text: qsTr("Continue")
                mdState.type: MD.Enum.BtFilled
                enabled: !root.waitingAddons
                onClicked: {
                    root.confirmed(root.entryId, root.entryTitle, root.selectedAddonIds())
                    root.close()
                }
            }
        }
    }
}
