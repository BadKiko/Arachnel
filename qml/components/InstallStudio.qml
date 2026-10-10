import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

// Installing a game without leaving the page. It replaces the chain of three bottom sheets (source,
// DLC, drive) with one panel that opens under the hero: DLC on the left, a summary on the right
// with the total size, the drive and the Install button.
Item {
    id: root

    required property var page
    property color accent: MD.Token.color.primary
    property color accentContainer: MD.Token.color.secondary_container
    property color accentInk: MD.Token.color.on_primary

    property bool open: false

    // The offer that will be downloaded (the same game can be offered by several sources).
    property var offers: []
    property int offerIndex: 0
    property string installId: ""
    property string sourceId: ""

    // DLC of the chosen offer and which of them are ticked.
    property var addons: []
    property var ticked: ({})
    property bool addonsLoading: false

    property string libraryId: ""
    readonly property bool needsLocation: Core.needsInstallLocationChoice()

    readonly property real baseBytes: {
        const o = offers[offerIndex]
        const label = o && o.sizeLabel ? o.sizeLabel : (Core.entryDetails(installId).sizeLabel || "")
        return parseBytes(label)
    }
    readonly property real dlcBytes: {
        let sum = 0
        for (let i = 0; i < addons.length; ++i) {
            if (ticked[addons[i].id])
                sum += parseBytes(addons[i].fileSize || "")
        }
        return sum
    }
    readonly property real totalBytes: baseBytes + dlcBytes
    readonly property int tickedCount: {
        let n = 0
        for (let i = 0; i < addons.length; ++i) {
            if (ticked[addons[i].id])
                ++n
        }
        return n
    }

    // The big number counts to the new total instead of jumping.
    property real shownBytes: totalBytes
    Behavior on shownBytes {
        NumberAnimation { duration: AppMotion.scaled(520); easing: AppMotion.emphasizedDecelerate }
    }

    signal opened()

    function parseBytes(label) {
        const m = /^(\d+(?:\.\d+)?)\s*(B|KB|MB|GB|TB)/i.exec(String(label || "").trim())
        if (!m)
            return 0
        const unit = m[2].toUpperCase()
        const pow = unit === "KB" ? 1 : unit === "MB" ? 2 : unit === "GB" ? 3 : unit === "TB" ? 4 : 0
        return parseFloat(m[1]) * Math.pow(1024, pow)
    }

    function formatBytes(bytes) {
        if (!(bytes > 0))
            return "—"
        const units = ["B", "KB", "MB", "GB", "TB"]
        let v = bytes
        let u = 0
        while (v >= 1024 && u < units.length - 1) {
            v /= 1024
            u++
        }
        return (u === 0 ? Math.round(v) : v.toFixed(1)) + " " + units[u]
    }

    function openStudio() {
        if (page.needsProtonCheck()) {
            page.protonRequired()
            return
        }
        offers = Core.installOffersForEntry(page.gameId) || []
        offerIndex = 0
        libraryId = Core.settings.storageLibraries.defaultLibraryId
        applyOffer()
        open = true
        opened()
    }

    function close() {
        open = false
    }

    function applyOffer() {
        const o = offers[offerIndex]
        installId = o && (o.entryId || "").length ? o.entryId : page.gameId
        sourceId = o ? (o.sourceId || "") : (page.info.sourceId || "")
        loadAddons()
    }

    function selectOffer(i) {
        if (i === offerIndex)
            return
        offerIndex = i
        applyOffer()
    }

    function loadAddons() {
        addons = []
        ticked = ({})
        addonsLoading = true
        const ready = Core.ensureCatalogAddons ? Core.ensureCatalogAddons(installId) : true
        if (ready)
            fillAddons()
        else
            addonsTimeout.restart()
    }

    function fillAddons() {
        addonsTimeout.stop()
        const list = Core.catalog.addonsFor(installId) || []
        const out = []
        const mark = {}
        for (let i = 0; i < list.length; ++i) {
            const a = list[i]
            if (!a || a.contentAvailable === false)
                continue
            const id = String(a.id)
            let cover = String(a.coverUrl || "")
            if (!/^(https?|file):/.test(cover) && id.indexOf("steam-") === 0) {
                const appId = id.slice(6)
                if (/^\d+$/.test(appId))
                    cover = "https://cdn.akamai.steamstatic.com/steam/apps/" + appId + "/header.jpg"
            }
            out.push({ id: id, title: a.title || id, fileSize: a.fileSize || "", coverUrl: cover })
            mark[id] = true
        }
        addons = out
        ticked = mark
        addonsLoading = false
    }

    function toggle(id) {
        const next = Object.assign({}, ticked)
        next[id] = !next[id]
        ticked = next
    }

    function setAll(on) {
        const next = {}
        for (let i = 0; i < addons.length; ++i)
            next[addons[i].id] = on
        ticked = next
    }

    function confirm() {
        const ids = []
        for (let i = 0; i < addons.length; ++i) {
            if (ticked[addons[i].id])
                ids.push(addons[i].id)
        }
        if (needsLocation) {
            if (sourceId.length)
                Core.installCatalogEntryFromSource(installId, sourceId, libraryId, ids)
            else
                Core.installCatalogEntry(installId, libraryId, ids)
        } else if (sourceId.length) {
            Core.installCatalogEntryFromSource(page.gameId, sourceId, "", ids)
        } else {
            Core.installCatalogEntry(installId, "", ids)
        }
        close()
    }

    Timer {
        id: addonsTimeout
        interval: 12000
        onTriggered: root.addonsLoading = false
    }

    Connections {
        target: Core
        function onCatalogAddonsReady(id) {
            if (root.open && root.addonsLoading && id === root.installId)
                root.fillAddons()
        }
    }

    // ---- Opening: the panel unfolds downwards ----
    property real reveal: open ? 1 : 0
    Behavior on reveal {
        NumberAnimation {
            duration: AppMotion.scaled(root.open ? 560 : 320)
            easing: root.open ? AppMotion.emphasizedDecelerate : AppMotion.emphasizedAccelerate
        }
    }

    visible: reveal > 0.001
    clip: true
    implicitHeight: Math.round(panel.implicitHeight * reveal)
    Layout.preferredHeight: implicitHeight

    readonly property bool wide: width >= 860

    Rectangle {
        id: panel
        width: parent.width
        implicitHeight: body.implicitHeight + 2 * MD.Token.spacing.large
        radius: 28
        color: MD.Token.color.surface_container
        opacity: Math.min(1, root.reveal * 1.5)

        GridLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: MD.Token.spacing.large
            columns: root.wide ? 2 : 1
            columnSpacing: MD.Token.spacing.large
            rowSpacing: MD.Token.spacing.large

            // ================= Left: what to download =================
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: MD.Token.spacing.medium

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Installation")
                        typescale: MD.Token.typescale.title_large
                    }
                    MD.IconButton {
                        visible: !root.wide
                        mdState.type: MD.Enum.IBtStandard
                        icon.name: MD.Token.icon.close
                        Accessible.name: qsTr("Cancel")
                        onClicked: root.close()
                    }
                }

                // Source, only when the game is offered by more than one.
                Flow {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small
                    visible: root.offers.length > 1

                    Repeater {
                        model: root.offers

                        delegate: MD.FilterChip {
                            required property var modelData
                            required property int index
                            text: (modelData.sourceName || modelData.sourceId || qsTr("Unknown source"))
                                  + ((modelData.sizeLabel || "").length ? "  ·  " + modelData.sizeLabel : "")
                            checkable: false
                            checked: root.offerIndex === index
                            onClicked: root.selectOffer(index)
                        }
                    }
                }

                // DLC header.
                RowLayout {
                    Layout.fillWidth: true
                    visible: root.addonsLoading || root.addons.length > 0
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.addonsLoading
                              ? qsTr("Loading DLC…")
                              : qsTr("DLC: %1 of %2").arg(root.tickedCount).arg(root.addons.length)
                        typescale: MD.Token.typescale.title_small
                        color: MD.Token.color.on_surface_variant
                    }
                    MD.Button {
                        visible: !root.addonsLoading && root.addons.length > 1
                        text: root.tickedCount === root.addons.length ? qsTr("Clear all") : qsTr("Select all")
                        mdState.type: MD.Enum.BtText
                        onClicked: root.setAll(root.tickedCount !== root.addons.length)
                    }
                }

                MD.Label {
                    Layout.fillWidth: true
                    visible: !root.addonsLoading && root.addons.length === 0
                    text: qsTr("This game has no DLC to choose. Everything is included.")
                    typescale: MD.Token.typescale.body_medium
                    color: MD.Token.color.on_surface_variant
                    wrapMode: Text.WordWrap
                }

                // DLC list: tidy rows with a thumbnail; the whole row toggles.
                Rectangle {
                    Layout.fillWidth: true
                    visible: root.addonsLoading || root.addons.length > 0
                    Layout.preferredHeight: Math.min(dlcList.contentHeight, 5 * 68) + 8
                    radius: 20
                    color: MD.Token.color.surface_container_low
                    clip: true

                    ListView {
                        id: dlcList
                        anchors.fill: parent
                        anchors.margins: 4
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: root.addonsLoading ? 3 : root.addons
                        spacing: 2

                        ScrollBar.vertical: MD.ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: Item {
                            id: row
                            required property var modelData
                            required property int index
                            readonly property bool skeleton: root.addonsLoading
                            readonly property bool on: !skeleton && !!root.ticked[modelData.id]

                            width: dlcList.width - (dlcList.contentHeight > dlcList.height ? 12 : 0)
                            height: 66

                            Rectangle {
                                anchors.fill: parent
                                radius: 16
                                color: row.on ? root.accentContainer
                                     : (rowHover.hovered ? MD.Token.color.surface_container_high : "transparent")
                                Behavior on color { ColorAnimation { duration: AppMotion.short } }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: MD.Token.spacing.small
                                anchors.rightMargin: MD.Token.spacing.medium
                                spacing: MD.Token.spacing.medium

                                // Check that pops in.
                                Rectangle {
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                    visible: !row.skeleton
                                    radius: row.on ? 8 : 12
                                    color: row.on ? root.accent : "transparent"
                                    border.width: row.on ? 0 : 2
                                    border.color: MD.Token.color.outline
                                    Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing: AppMotion.emphasizedDecelerate } }
                                    Behavior on color { ColorAnimation { duration: AppMotion.short } }

                                    MD.Icon {
                                        anchors.centerIn: parent
                                        name: MD.Token.icon.check
                                        size: 18
                                        color: root.accentInk
                                        scale: row.on ? 1 : 0
                                        Behavior on scale {
                                            NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack; easing.overshoot: 2.2 }
                                        }
                                    }
                                }

                                Item {
                                    Layout.preferredWidth: 104
                                    Layout.preferredHeight: 48

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 10
                                        color: MD.Token.color.surface_container_highest
                                    }
                                    MD.Image {
                                        anchors.fill: parent
                                        radius: 10
                                        visible: !row.skeleton
                                        source: row.skeleton ? "" : row.modelData.coverUrl
                                        sourceSize: Qt.size(230, 108)
                                        asynchronous: true
                                    }
                                    CatalogShimmerBox {
                                        anchors.fill: parent
                                        visible: row.skeleton
                                        cornerRadius: 10
                                        running: row.skeleton && root.open
                                    }
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: row.skeleton ? "" : row.modelData.title
                                    typescale: MD.Token.typescale.body_large
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                }

                                MD.Label {
                                    text: row.skeleton || root.parseBytes(row.modelData.fileSize) <= 0
                                          ? "" : row.modelData.fileSize
                                    typescale: MD.Token.typescale.label_large
                                    color: MD.Token.color.on_surface_variant
                                }
                            }

                            HoverHandler { id: rowHover; cursorShape: row.skeleton ? Qt.ArrowCursor : Qt.PointingHandCursor }
                            TapHandler {
                                enabled: !row.skeleton
                                onTapped: root.toggle(row.modelData.id)
                            }
                        }

                        add: Transition {
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppMotion.medium }
                            NumberAnimation { property: "x"; from: 24; to: 0; duration: AppMotion.long; easing: AppMotion.emphasizedDecelerate }
                        }
                    }
                }
            }

            // ================= Right: summary =================
            Rectangle {
                Layout.fillWidth: !root.wide
                Layout.preferredWidth: root.wide ? 340 : -1
                Layout.alignment: Qt.AlignTop
                implicitHeight: summary.implicitHeight + 2 * MD.Token.spacing.large
                radius: 24
                color: MD.Token.color.surface_container_high

                ColumnLayout {
                    id: summary
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.large
                    spacing: MD.Token.spacing.medium

                    RowLayout {
                        Layout.fillWidth: true

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Space needed")
                            typescale: MD.Token.typescale.label_large
                            color: MD.Token.color.on_surface_variant
                        }
                        MD.IconButton {
                            visible: root.wide
                            mdState.type: MD.Enum.IBtStandard
                            icon.name: MD.Token.icon.close
                            Accessible.name: qsTr("Cancel")
                            onClicked: root.close()
                        }
                    }

                    Text {
                        text: root.formatBytes(root.shownBytes)
                        color: MD.Token.color.on_surface
                        font.family: sizeFont.font.family
                        font.pixelSize: 40
                        font.weight: Font.DemiBold

                        MD.Label { id: sizeFont; visible: false }
                    }

                    // Game and DLC share of the total.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 10
                        radius: 5
                        color: MD.Token.color.surface_container_highest
                        visible: root.totalBytes > 0

                        Rectangle {
                            id: gameSeg
                            height: parent.height
                            radius: 5
                            color: root.accent
                            width: root.totalBytes > 0 ? parent.width * (root.baseBytes / root.totalBytes) : 0
                            Behavior on width { NumberAnimation { duration: AppMotion.scaled(520); easing: AppMotion.emphasizedDecelerate } }
                        }
                        Rectangle {
                            height: parent.height
                            radius: 5
                            x: gameSeg.width + (root.dlcBytes > 0 ? 3 : 0)
                            width: root.totalBytes > 0 ? Math.max(0, parent.width * (root.dlcBytes / root.totalBytes) - 3) : 0
                            color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.45)
                            Behavior on width { NumberAnimation { duration: AppMotion.scaled(520); easing: AppMotion.emphasizedDecelerate } }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: MD.Token.spacing.small
                        rowSpacing: 2

                        RowLayout {
                            spacing: 6
                            Rectangle { width: 8; height: 8; radius: 4; color: root.accent }
                            MD.Label { text: qsTr("Game"); typescale: MD.Token.typescale.body_medium; color: MD.Token.color.on_surface_variant }
                        }
                        MD.Label {
                            Layout.alignment: Qt.AlignRight
                            text: root.formatBytes(root.baseBytes)
                            typescale: MD.Token.typescale.body_medium
                        }
                        RowLayout {
                            visible: root.addons.length > 0
                            spacing: 6
                            Rectangle { width: 8; height: 8; radius: 4; color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.45) }
                            MD.Label { text: qsTr("DLC"); typescale: MD.Token.typescale.body_medium; color: MD.Token.color.on_surface_variant }
                        }
                        MD.Label {
                            visible: root.addons.length > 0
                            Layout.alignment: Qt.AlignRight
                            text: root.tickedCount > 0 ? root.formatBytes(root.dlcBytes) : "0"
                            typescale: MD.Token.typescale.body_medium
                        }
                    }

                    // Drive picker.
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: MD.Token.spacing.small
                        visible: root.needsLocation
                        spacing: 4

                        MD.Label {
                            text: qsTr("Install to")
                            typescale: MD.Token.typescale.label_large
                            color: MD.Token.color.on_surface_variant
                        }

                        Repeater {
                            model: Core.settings.storageLibraries

                            delegate: Rectangle {
                                id: lib
                                required property string libraryId
                                required property string label
                                required property string path
                                required property bool isDefault
                                readonly property bool picked: root.libraryId === libraryId

                                Layout.fillWidth: true
                                implicitHeight: 56
                                radius: 16
                                color: picked ? root.accentContainer
                                     : (libHover.hovered ? MD.Token.color.surface_container_highest : "transparent")
                                Behavior on color { ColorAnimation { duration: AppMotion.short } }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: MD.Token.spacing.small
                                    anchors.rightMargin: MD.Token.spacing.small
                                    spacing: MD.Token.spacing.small

                                    // Radio dot.
                                    Rectangle {
                                        Layout.preferredWidth: 20
                                        Layout.preferredHeight: 20
                                        radius: 10
                                        color: "transparent"
                                        border.width: 2
                                        border.color: lib.picked ? root.accent : MD.Token.color.outline

                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 10
                                            height: 10
                                            radius: 5
                                            color: root.accent
                                            scale: lib.picked ? 1 : 0
                                            Behavior on scale {
                                                NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack; easing.overshoot: 2.0 }
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        RowLayout {
                                            spacing: 4
                                            MD.Label {
                                                text: lib.label
                                                typescale: MD.Token.typescale.title_small
                                            }
                                            MD.Icon {
                                                visible: lib.isDefault
                                                name: MD.Token.icon.star
                                                size: 14
                                                color: root.accent
                                            }
                                        }
                                        MD.Label {
                                            Layout.fillWidth: true
                                            text: lib.path
                                            typescale: MD.Token.typescale.label_small
                                            color: MD.Token.color.on_surface_variant
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                }

                                HoverHandler { id: libHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: root.libraryId = lib.libraryId }
                            }
                        }
                    }

                    MD.Button {
                        Layout.fillWidth: true
                        Layout.topMargin: MD.Token.spacing.small
                        text: qsTr("Install")
                        icon.name: MD.Token.icon.download
                        mdState.type: MD.Enum.BtFilled
                        mdState.size: MD.Enum.M
                        mdState.backgroundColor: root.accent
                        mdState.textColor: root.accentInk
                        enabled: !root.addonsLoading && (!root.needsLocation || root.libraryId.length > 0)
                        onClicked: root.confirm()
                    }
                }
            }
        }
    }
}
