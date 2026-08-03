import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.BottomSheet {
    id: root

    sheetType: MD.Enum.BottomSheetModal
    property string gameId: ""
    property var item: ({})
    property int libraryTick: 0

    readonly property var shotUrls: {
        const raw = item.previewUrls
        if (raw && raw.length) {
            const out = []
            for (let i = 0; i < raw.length; ++i) {
                const u = String(raw[i] || "")
                if (u.length)
                    out.push(u)
            }
            if (out.length)
                return out
        }
        const local = item.localPreviewUrl || Core.workshopPreviewUrl(item.previewUrl || "")
        if (local && String(local).startsWith("file:"))
            return [local]
        const remote = String(item.previewUrl || "")
        return remote.length ? [remote] : []
    }

    readonly property bool mediaLoading: !!(item.publishedFileId)
                                         && item.screenshotsResolved !== true

    readonly property string itemStatus: {
        void libraryTick
        if (!gameId.length || !(item.publishedFileId))
            return ""
        return Core.workshopItemStatus(gameId, String(item.publishedFileId)) || ""
    }

    readonly property bool gameInstalled: {
        void libraryTick
        const info = Core.library.gameInfo(gameId)
        return !!(info && info.installPath)
    }

    function openFor(gameIdValue, workshopItem) {
        gameId = gameIdValue || ""
        item = workshopItem || ({})
        libraryTick++
        const urls = item.previewUrls
        if (urls && urls.length) {
            for (let i = 0; i < urls.length; ++i) {
                if (urls[i])
                    Core.requestWorkshopPreview(String(urls[i]))
            }
        } else if ((item.previewUrl || "").length) {
            Core.requestWorkshopPreview(item.previewUrl)
        }
        open()
    }

    Connections {
        target: Core.library
        function onCountChanged() { root.libraryTick++ }
        function onLibraryChanged() { root.libraryTick++ }
    }

    ColumnLayout {
        width: root.sheetWidth
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.topMargin: MD.Token.spacing.medium
            text: root.item.title || qsTr("Workshop file")
            typescale: MD.Token.typescale.headline_small
            wrapMode: Text.WordWrap
        }

        GameDetailsMediaSection {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            screenshotUrls: root.shotUrls
            trailerUrl: ""
            trailerThumbnailUrl: ""
            loading: root.mediaLoading
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: !!(root.item.sizeLabel)
            text: root.item.sizeLabel || ""
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.label_large
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: root.itemStatus === "installed" || root.itemStatus === "cached"
            text: root.itemStatus === "installed" ? qsTr("Installed")
                  : root.itemStatus === "cached" ? qsTr("Cached")
                  : ""
            color: MD.Token.color.primary
            typescale: MD.Token.typescale.label_large
        }

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            visible: !!(root.item.description && String(root.item.description).length)
            text: root.item.description || ""
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
            wrapMode: Text.WordWrap
            maximumLineCount: 8
            elide: Text.ElideRight
        }

        MD.Button {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: root.itemStatus === "installed" ? MD.Token.spacing.medium : 0
            visible: root.itemStatus === "cached" && root.gameInstalled
            text: qsTr("Attach to game")
            mdState.type: MD.Enum.BtFilled
            enabled: root.gameId.length > 0 && !!(root.item.publishedFileId)
            onClicked: {
                Core.attachWorkshopItem(root.gameId, root.item.publishedFileId)
                root.libraryTick++
                root.close()
            }
        }

        MD.Button {
            Layout.fillWidth: true
            Layout.leftMargin: MD.Token.spacing.large
            Layout.rightMargin: MD.Token.spacing.large
            Layout.bottomMargin: MD.Token.spacing.medium
            visible: root.itemStatus !== "installed"
            text: root.itemStatus === "cached" ? qsTr("Download again") : qsTr("Download")
            mdState.type: root.itemStatus === "cached" && root.gameInstalled
                          ? MD.Enum.BtOutlined
                          : MD.Enum.BtFilled
            enabled: root.gameId.length > 0 && !!(root.item.publishedFileId)
            onClicked: {
                Core.downloadWorkshopItem(root.gameId, root.item.publishedFileId)
                root.close()
            }
        }
    }
}
