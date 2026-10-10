import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

// Installing a game without leaving the page. It replaces the chain of three bottom sheets (source,
// DLC, drive) with one card that grows out of the action dock: everything that decides the install
// is on screen at once, the total size counts up as DLC is ticked, and the drive is one tap.
Item {
    id: root

    required property var page

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
            if (!cover.length && id.indexOf("steam-") === 0) {
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
        const lib = needsLocation ? libraryId : ""
        if (needsLocation) {
            if (sourceId.length)
                Core.installCatalogEntryFromSource(installId, sourceId, lib, ids)
            else
                Core.installCatalogEntry(installId, lib, ids)
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

    // ---- Geometry: the card grows out of the dock ----
    property real reveal: open ? 1 : 0
    Behavior on reveal {
        NumberAnimation {
            duration: AppMotion.scaled(open ? 640 : 380)
            easing: open ? AppMotion.emphasizedDecelerate : AppMotion.emphasizedAccelerate
        }
    }

    visible: reveal > 0.001
    clip: true
    implicitHeight: Math.round(card.implicitHeight * reveal)
    Layout.preferredHeight: implicitHeight

    Rectangle {
        id: card
        width: parent.width
        implicitHeight: body.implicitHeight + MD.Token.spacing.extra_large * 2
        // The corners start tight, like the dock, and soften as the card opens.
        radius: 12 + 16 * root.reveal
        color: MD.Token.color.surface_container_high
        border.width: 1
        border.color: MD.Token.color.outline_variant
        opacity: Math.min(1, root.reveal * 1.6)
        transform: Translate { y: (1 - root.reveal) * -28 }

        ColumnLayout {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: MD.Token.spacing.extra_large
            spacing: MD.Token.spacing.large

            // ---------- Header ----------
            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.medium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    MD.Label {
                        text: qsTr("Install").toUpperCase()
                        typescale: MD.Token.typescale.label_large
                        font.letterSpacing: 2
                        color: MD.Token.color.primary
                    }
                    MD.Label {
                        Layout.fillWidth: true
                        text: page.info.title ?? ""
                        typescale: MD.Token.typescale.title_large
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                }

                ColumnLayout {
                    spacing: 0
                    Layout.alignment: Qt.AlignRight

                    MD.Label {
                        Layout.alignment: Qt.AlignRight
                        text: root.formatBytes(root.shownBytes)
                        typescale: MD.Token.typescale.headline_large
                        color: MD.Token.color.on_surface
                    }
                    MD.Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("on disk")
                        typescale: MD.Token.typescale.label_medium
                        color: MD.Token.color.on_surface_variant
                    }
                }

                MD.IconButton {
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.close
                    Accessible.name: qsTr("Cancel")
                    onClicked: root.close()
                }
            }

            // ---------- Size bar: game and DLC ----------
            ColumnLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.extra_small
                visible: root.totalBytes > 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 12
                    radius: 6
                    color: MD.Token.color.surface_container_highest
                    clip: true

                    Rectangle {
                        id: gameSeg
                        height: parent.height
                        radius: 6
                        color: MD.Token.color.primary
                        width: root.totalBytes > 0 ? parent.width * (root.baseBytes / root.totalBytes) : 0
                        Behavior on width {
                            NumberAnimation { duration: AppMotion.scaled(520); easing: AppMotion.emphasizedDecelerate }
                        }
                    }
                    Rectangle {
                        height: parent.height
                        radius: 6
                        x: gameSeg.width - 6
                        width: root.totalBytes > 0 ? parent.width * (root.dlcBytes / root.totalBytes) + 6 : 0
                        color: MD.Token.color.tertiary
                        Behavior on width {
                            NumberAnimation { duration: AppMotion.scaled(520); easing: AppMotion.emphasizedDecelerate }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.large

                    RowLayout {
                        spacing: MD.Token.spacing.extra_small
                        Rectangle { width: 10; height: 10; radius: 5; color: MD.Token.color.primary }
                        MD.Label {
                            text: qsTr("Game %1").arg(root.formatBytes(root.baseBytes))
                            typescale: MD.Token.typescale.label_medium
                            color: MD.Token.color.on_surface_variant
                        }
                    }
                    RowLayout {
                        visible: root.tickedCount > 0
                        spacing: MD.Token.spacing.extra_small
                        Rectangle { width: 10; height: 10; radius: 5; color: MD.Token.color.tertiary }
                        MD.Label {
                            text: qsTr("DLC %1").arg(root.formatBytes(root.dlcBytes))
                            typescale: MD.Token.typescale.label_medium
                            color: MD.Token.color.on_surface_variant
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
            }

            // ---------- Source ----------
            ColumnLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.small
                visible: root.offers.length > 1

                MD.Label {
                    text: qsTr("Download source")
                    typescale: MD.Token.typescale.title_small
                    color: MD.Token.color.on_surface_variant
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    Repeater {
                        model: root.offers

                        delegate: Rectangle {
                            id: offerCard
                            required property var modelData
                            required property int index
                            readonly property bool picked: root.offerIndex === index

                            implicitWidth: offerRow.implicitWidth + MD.Token.spacing.large * 2
                            implicitHeight: 64
                            radius: picked ? 16 : 28
                            color: picked ? MD.Token.color.secondary_container
                                          : MD.Token.color.surface_container_highest
                            border.width: picked ? 2 : 1
                            border.color: picked ? MD.Token.color.primary : MD.Token.color.outline_variant
                            scale: tap.pressed ? 0.97 : 1

                            Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing: AppMotion.emphasizedDecelerate } }
                            Behavior on color { ColorAnimation { duration: AppMotion.short } }
                            Behavior on scale { NumberAnimation { duration: AppMotion.fast } }

                            RowLayout {
                                id: offerRow
                                anchors.centerIn: parent
                                spacing: MD.Token.spacing.small

                                MD.Icon {
                                    name: offerCard.picked ? MD.Token.icon.check_circle : MD.Token.icon.storefront
                                    size: 22
                                    color: offerCard.picked ? MD.Token.color.primary : MD.Token.color.on_surface_variant
                                }
                                ColumnLayout {
                                    spacing: 0
                                    MD.Label {
                                        text: offerCard.modelData.sourceName || offerCard.modelData.sourceId
                                              || qsTr("Unknown source")
                                        typescale: MD.Token.typescale.title_small
                                    }
                                    MD.Label {
                                        visible: !!(offerCard.modelData.sizeLabel)
                                        text: offerCard.modelData.sizeLabel || ""
                                        typescale: MD.Token.typescale.label_small
                                        color: MD.Token.color.on_surface_variant
                                    }
                                }
                            }

                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                id: tap
                                onTapped: root.selectOffer(offerCard.index)
                            }
                        }
                    }
                }
            }

            // ---------- DLC ----------
            ColumnLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.small
                visible: root.addonsLoading || root.addons.length > 0

                RowLayout {
                    Layout.fillWidth: true

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.addonsLoading
                              ? qsTr("Loading DLC from Steam…")
                              : qsTr("DLC · %1 of %2").arg(root.tickedCount).arg(root.addons.length)
                        typescale: MD.Token.typescale.title_small
                        color: MD.Token.color.on_surface_variant
                    }
                    MD.Button {
                        visible: !root.addonsLoading && root.addons.length > 1
                        text: root.tickedCount === root.addons.length ? qsTr("None") : qsTr("All")
                        mdState.type: MD.Enum.BtText
                        onClicked: root.setAll(root.tickedCount !== root.addons.length)
                    }
                }

                ListView {
                    id: dlcList
                    Layout.fillWidth: true
                    Layout.preferredHeight: 176
                    orientation: ListView.Horizontal
                    spacing: MD.Token.spacing.small
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.addonsLoading ? 4 : root.addons
                    // Wheel scrolls the strip sideways, like a carousel.
                    WheelHandler {
                        acceptedDevices: PointerDevice.Mouse
                        onWheel: function (e) {
                            dlcList.contentX = Math.max(0, Math.min(dlcList.contentWidth - dlcList.width,
                                                                    dlcList.contentX - e.angleDelta.y))
                        }
                    }
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }

                    delegate: Item {
                        id: dlc
                        required property var modelData
                        required property int index
                        readonly property bool skeleton: root.addonsLoading
                        readonly property bool on: !skeleton && !!root.ticked[modelData.id]

                        width: 232
                        height: dlcList.height

                        Rectangle {
                            id: dlcCard
                            anchors.fill: parent
                            radius: dlc.on ? 20 : 28
                            color: dlc.on ? MD.Token.color.secondary_container
                                          : MD.Token.color.surface_container_highest
                            border.width: dlc.on ? 2 : 1
                            border.color: dlc.on ? MD.Token.color.primary : MD.Token.color.outline_variant
                            scale: dlcTap.pressed ? 0.97 : 1

                            Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing: AppMotion.emphasizedDecelerate } }
                            Behavior on color { ColorAnimation { duration: AppMotion.short } }
                            Behavior on scale { NumberAnimation { duration: AppMotion.fast } }

                            Item {
                                id: art
                                x: 8
                                y: 8
                                width: parent.width - 16
                                height: 100
                                layer.enabled: true
                                layer.effect: MD.RoundClip { radius: 16 }

                                Rectangle {
                                    anchors.fill: parent
                                    color: MD.Token.color.surface_container
                                }
                                MD.Icon {
                                    anchors.centerIn: parent
                                    visible: !dlc.skeleton && art_img.status !== Image.Ready
                                    name: MD.Token.icon.extension
                                    size: 36
                                    color: MD.Token.color.on_surface_variant
                                    opacity: 0.6
                                }
                                Image {
                                    id: art_img
                                    anchors.fill: parent
                                    visible: !dlc.skeleton
                                    source: dlc.skeleton ? "" : dlc.modelData.coverUrl
                                    sourceSize: Qt.size(464, 200)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: true
                                    scale: dlcTap.pressed ? 1.0 : (dlcHover.hovered ? 1.06 : 1.0)
                                    Behavior on scale { NumberAnimation { duration: AppMotion.medium; easing: AppMotion.emphasizedDecelerate } }
                                }
                                CatalogShimmerBox {
                                    anchors.fill: parent
                                    visible: dlc.skeleton
                                    cornerRadius: 0
                                    running: dlc.skeleton && root.open
                                }
                            }

                            // Tick badge: the check pops in with a little overshoot.
                            Rectangle {
                                visible: !dlc.skeleton
                                x: parent.width - width - 16
                                y: 16
                                width: 30
                                height: 30
                                radius: 15
                                color: dlc.on ? MD.Token.color.primary : Qt.rgba(0, 0, 0, 0.45)
                                border.width: dlc.on ? 0 : 2
                                border.color: "white"
                                Behavior on color { ColorAnimation { duration: AppMotion.short } }

                                MD.Icon {
                                    anchors.centerIn: parent
                                    name: MD.Token.icon.check
                                    size: 20
                                    color: MD.Token.color.on_primary
                                    scale: dlc.on ? 1 : 0
                                    Behavior on scale {
                                        NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack; easing.overshoot: 2.4 }
                                    }
                                }
                            }

                            ColumnLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: art.bottom
                                anchors.margins: MD.Token.spacing.small
                                anchors.topMargin: MD.Token.spacing.small
                                spacing: 2

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: dlc.skeleton ? "" : dlc.modelData.title
                                    typescale: MD.Token.typescale.title_small
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                                MD.Label {
                                    Layout.fillWidth: true
                                    text: dlc.skeleton || root.parseBytes(dlc.modelData.fileSize) <= 0
                                          ? "" : dlc.modelData.fileSize
                                    typescale: MD.Token.typescale.label_small
                                    color: MD.Token.color.on_surface_variant
                                }
                            }

                            HoverHandler { id: dlcHover; cursorShape: dlc.skeleton ? Qt.ArrowCursor : Qt.PointingHandCursor }
                            TapHandler {
                                id: dlcTap
                                enabled: !dlc.skeleton
                                onTapped: root.toggle(dlc.modelData.id)
                            }
                        }
                    }

                    // Cards settle in one after another when the card opens.
                    add: Transition {
                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppMotion.medium }
                        NumberAnimation { property: "y"; from: 24; to: 0; duration: AppMotion.long; easing: AppMotion.emphasizedDecelerate }
                    }
                }
            }

            // ---------- Drive ----------
            ColumnLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.small
                visible: root.needsLocation

                MD.Label {
                    text: qsTr("Install to")
                    typescale: MD.Token.typescale.title_small
                    color: MD.Token.color.on_surface_variant
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    Repeater {
                        model: Core.settings.storageLibraries

                        delegate: Rectangle {
                            id: lib
                            required property string libraryId
                            required property string label
                            required property string path
                            required property bool isDefault
                            readonly property bool picked: root.libraryId === libraryId

                            implicitWidth: Math.max(240, libRow.implicitWidth + MD.Token.spacing.large * 2)
                            implicitHeight: 72
                            radius: picked ? 16 : 28
                            color: picked ? MD.Token.color.secondary_container
                                          : MD.Token.color.surface_container_highest
                            border.width: picked ? 2 : 1
                            border.color: picked ? MD.Token.color.primary : MD.Token.color.outline_variant
                            scale: libTap.pressed ? 0.97 : 1

                            Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing: AppMotion.emphasizedDecelerate } }
                            Behavior on color { ColorAnimation { duration: AppMotion.short } }
                            Behavior on scale { NumberAnimation { duration: AppMotion.fast } }

                            RowLayout {
                                id: libRow
                                anchors.fill: parent
                                anchors.leftMargin: MD.Token.spacing.large
                                anchors.rightMargin: MD.Token.spacing.large
                                spacing: MD.Token.spacing.medium

                                Rectangle {
                                    Layout.preferredWidth: 40
                                    Layout.preferredHeight: 40
                                    radius: lib.picked ? 12 : 20
                                    color: lib.picked ? MD.Token.color.primary : MD.Token.color.surface_container
                                    Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing: AppMotion.emphasizedDecelerate } }
                                    Behavior on color { ColorAnimation { duration: AppMotion.short } }

                                    MD.Icon {
                                        anchors.centerIn: parent
                                        name: MD.Token.icon.hard_drive
                                        size: 22
                                        color: lib.picked ? MD.Token.color.on_primary : MD.Token.color.on_surface_variant
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    RowLayout {
                                        spacing: MD.Token.spacing.extra_small
                                        MD.Label {
                                            text: lib.label
                                            typescale: MD.Token.typescale.title_small
                                        }
                                        MD.Icon {
                                            visible: lib.isDefault
                                            name: MD.Token.icon.star
                                            size: 16
                                            color: MD.Token.color.primary
                                        }
                                    }
                                    MD.Label {
                                        Layout.fillWidth: true
                                        Layout.maximumWidth: 260
                                        text: lib.path
                                        typescale: MD.Token.typescale.label_small
                                        color: MD.Token.color.on_surface_variant
                                        elide: Text.ElideMiddle
                                    }
                                }
                            }

                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                id: libTap
                                onTapped: root.libraryId = lib.libraryId
                            }
                        }
                    }
                }
            }

            // ---------- Footer ----------
            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.medium

                Item { Layout.fillWidth: true }

                MD.Button {
                    text: qsTr("Cancel")
                    mdState.type: MD.Enum.BtText
                    onClicked: root.close()
                }

                MD.Button {
                    text: root.totalBytes > 0
                          ? qsTr("Install · %1").arg(root.formatBytes(root.totalBytes))
                          : qsTr("Install")
                    icon.name: MD.Token.icon.download
                    mdState.type: MD.Enum.BtFilled
                    mdState.size: MD.Enum.L
                    enabled: !root.addonsLoading
                             && (!root.needsLocation || root.libraryId.length > 0)
                    onClicked: root.confirm()
                }
            }
        }
    }
}
