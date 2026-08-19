import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Window

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.ApplicationWindow {
    id: root

    visible: true
    width: 1450
    height: 900
    minimumWidth: 1100
    minimumHeight: 720
    title: qsTr("Arachnel")
    color: MD.Token.color.surface_container
    flags: customTitleBar ? (Qt.Window | Qt.FramelessWindowHint) : Qt.Window

    readonly property bool customTitleBar: Qt.platform.os === "windows"

    MD.MProp.textColor: MD.MProp.color.on_surface
    MD.MProp.backgroundColor: MD.MProp.color.surface_container

    property int windowClass: MD.Token.window_class.select_type(width)
    MD.MProp.size.windowClass: windowClass

    Timer {
        id: windowClassTimer
        interval: 200
        onTriggered: {
            const next = MD.Token.window_class.select_type(root.width)
            if (next !== root.windowClass)
                root.windowClass = next
        }
    }
    onWidthChanged: windowClassTimer.restart()

    property int pageIndex: 0
    property bool detailsOpen: pageStack.depth > 1
    property string detailsGameId: ""
    property bool detailsFromCatalog: false
    readonly property int downloadBadge: Core.jobs.activeCount

    function goToPage(index) {
        if (pageStack.depth > 1)
            pageStack.navigateToRoot()

        root.detailsGameId = ""
        root.pageIndex = index
        // Tab opacity crossfade can leave Discover/Catalog scrolled under the clip.
        Qt.callLater(function () {
            if (pageStack.currentItem && pageStack.currentItem.fixCatalogViewports)
                pageStack.currentItem.fixCatalogViewports()
        })
    }

    function openCatalogWithQuery(query) {
        // Discover "All games" / search / filters → Catalog tab in the rail.
        root.goToPage(2)
        const q = query || ""
        function tryShow(attempt) {
            const pages = pageStack.currentItem
            const page = pages && pages.catalogBrowsePage
            if (page) {
                page.showBrowseAll(q)
                return
            }
            if (attempt < 30)
                Qt.callLater(function () { tryShow(attempt + 1) })
        }
        Qt.callLater(function () { tryShow(0) })
    }

    function openGameDetails(gameId, fromCatalog) {
        // Capture before push - StackView hide can zero catalog contentY.
        if (fromCatalog && pageStack.currentItem
                && pageStack.currentItem.captureActiveCatalogScroll)
            pageStack.currentItem.captureActiveCatalogScroll(gameId)
        root.detailsGameId = gameId
        root.detailsFromCatalog = !!fromCatalog
        pageStack.navigatePush(detailsPageComponent, {
                                   "gameId": gameId,
                                   "fromCatalog": !!fromCatalog
                               })
    }

    function raiseMainWindow() {
        if (root.visibility === Window.Minimized || root.visibility === Window.Hidden)
            root.showNormal()
        else
            root.show()
        root.raise()
        root.requestActivate()
        // Windows often blocks Qt raise when focus came from a second instance;
        // Core retries with native SetForegroundWindow.
        Core.forceActivateMainWindow()
    }

    function handleDeepLink(gameId) {
        const id = (gameId || "").trim()
        if (!id.length)
            return
        root.raiseMainWindow()
        root.openGameDetails(id, true)
        Core.consumePendingDeepLink()
    }

    function closeGameDetails() {
        if (pageStack.canPop)
            pageStack.navigatePop()
        root.detailsGameId = ""
        Qt.callLater(function () {
            if (pageStack.currentItem && pageStack.currentItem.fixCatalogViewports)
                pageStack.currentItem.fixCatalogViewports()
            if (pageStack.currentItem && pageStack.currentItem.restoreActiveCatalogScroll)
                pageStack.currentItem.restoreActiveCatalogScroll()
        })
    }

    function addonIdsForInstall(addonIds) {
        const out = []
        if (!addonIds)
            return out
        const len = addonIds.length !== undefined ? addonIds.length : 0
        for (let i = 0; i < len; ++i) {
            const id = String(addonIds[i] || "").trim()
            if (id.length)
                out.push(id)
        }
        return out
    }

    function beginCatalogInstall(entryId, libraryId, addonIds, sourceId) {
        if (Core.needsProtonOnPlatform() && !Core.protonReady) {
            protonRequiredDialog.open()
            return
        }
        const sid = sourceId || ""
        if (sid.length)
            Core.installCatalogEntryFromSource(entryId, sid, libraryId || "",
                                               root.addonIdsForInstall(addonIds))
        else
            Core.installCatalogEntry(entryId, libraryId || "", root.addonIdsForInstall(addonIds))
    }

    Component.onCompleted: {
        Appearance.apply()
        if (Qt.platform.os === "linux")
            Core.refreshProtonLatestRelease()
        if (!Core.settings.onboardingCompleted)
            Qt.callLater(function () { onboardingSheet.openWizard() })
        else if (Core.hasPendingCrashReport())
            Qt.callLater(function () { crashReportDialog.open() })
        // After first paint, warm Discover/Catalog so the first rail click isn't a cold create.
        catalogWarmTimer.start()
        if ((Core.pendingDeepLinkGameId || "").length > 0) {
            const id = Core.pendingDeepLinkGameId
            Qt.callLater(function () { root.handleDeepLink(id) })
        }
    }

    Timer {
        id: catalogWarmTimer
        interval: 600
        repeat: false
        onTriggered: {
            // Don't instantiate Catalog/GridView while a huge merge is about to land.
            if (Core.catalogLoading) {
                catalogWarmTimer.interval = 800
                catalogWarmTimer.start()
                return
            }
            if (pageStack.currentItem && pageStack.currentItem.warmCatalogLoaders)
                pageStack.currentItem.warmCatalogLoaders()
        }
    }

    // Hang watchdog writes a pending report while the app is still alive.
    Timer {
        interval: 4000
        running: true
        repeat: true
        onTriggered: {
            if (!Core.hasPendingCrashReport())
                return
            if (crashReportDialog.opened || crashReportDialog.visible)
                return
            if (onboardingSheet.visible)
                return
            crashReportDialog.open()
        }
    }

    onClosing: function (close) {
        close.accepted = true
        Qt.quit()
    }

    Connections {
        target: Core
        function onUserNoticeChanged() {
            if (Core.userNotice.length > 0)
                snackbar.show(Core.userNotice)
        }
        function onDeepLinkRequested(gameId) {
            root.handleDeepLink(gameId)
        }
        function onActivationRequested() {
            root.raiseMainWindow()
        }
    }

    Connections {
        target: Core.appUpdater
        function onUpdateCheckFinished(available, latestVersion) {
            if (available && !Core.appUpdater.downloading)
                appUpdateSheet.openForVersion(latestVersion)
        }
    }

    readonly property var navModel: [
        {
            name: qsTr("Library"),
            icon: MD.Token.icon.sports_esports
        },
        {
            name: qsTr("Discover"),
            icon: MD.Token.icon.auto_awesome
        },
        {
            name: qsTr("Catalog"),
            icon: MD.Token.icon.storefront
        },
        {
            name: qsTr("Friends"),
            icon: MD.Token.icon.groups
        },
        {
            name: qsTr("Favorites"),
            icon: MD.Token.icon.favorite
        },
        {
            name: qsTr("Downloads"),
            icon: MD.Token.icon.downloading,
            showDownloadBadge: true
        }
    ]

    // Main rail tabs stay mounted after first open; keep the crossfade snappy.
    readonly property int mainTabDuration: MD.Token.duration.short2

    Component {
        id: mainPagesComponent
        Item {
            id: mainPages

            // Always follow the window rail index. Writing this property used to
            // break the binding and leave Discover visible while Catalog was selected.
            readonly property int pageIndex: root.pageIndex
            // Loader.item — null until Catalog tab is opened once.
            readonly property var catalogBrowsePage: catalogBrowseLoader.item
            readonly property var discoverPage: discoverLoader.item

            transformOrigin: Item.Center

            function activeCatalogPage() {
                if (mainPages.pageIndex === 1)
                    return discoverLoader.item
                if (mainPages.pageIndex === 2)
                    return catalogBrowseLoader.item
                return null
            }

            function captureActiveCatalogScroll(entryId) {
                const page = mainPages.activeCatalogPage()
                if (page && page.captureBrowseScroll)
                    page.captureBrowseScroll(entryId || "")
            }

            function restoreActiveCatalogScroll() {
                const page = mainPages.activeCatalogPage()
                if (page && page.restoreBrowseScroll)
                    page.restoreBrowseScroll()
            }

            function fixCatalogViewports() {
                // Discover + Catalog tabs both host scrollable headers.
                for (let i = 0; i < children.length; ++i) {
                    const child = children[i]
                    if (child && child.fixViewport)
                        child.fixViewport()
                    // Loader hosts CatalogPage as item.
                    if (child && child.item && child.item.fixViewport)
                        child.item.fixViewport()
                }
            }

            function warmCatalogLoaders() {
                // Create Discover/Catalog off-tab (enabled=false → catalog model stays null).
                if (!discoverLoader.keepAlive) {
                    discoverLoader.keepAlive = true
                    discoverLoader.active = true
                }
                if (!catalogBrowseLoader.keepAlive) {
                    catalogBrowseLoader.keepAlive = true
                    catalogBrowseLoader.active = true
                }
            }

            StackView.onStatusChanged: {
                if (StackView.status === StackView.Active) {
                    opacity = 1
                    scale = 1
                    Qt.callLater(function () {
                        mainPages.fixCatalogViewports()
                        mainPages.restoreActiveCatalogScroll()
                    })
                }
            }

            // Soft opacity crossfade only. Scale on mounted Flickables (Discover
            // shelves) throws contentY under the pane clip after tab switches.
            LibraryPage {
                anchors.fill: parent
                opacity: mainPages.pageIndex === 0 ? 1 : 0
                enabled: mainPages.pageIndex === 0 && opacity > 0.99
                onOpenGame: function (id) { root.openGameDetails(id, false) }
                onOpenCatalog: root.goToPage(2)
                onOpenDownloads: root.goToPage(5)
                onOpenSettings: settingsSheet.openSettings()
                onAddSourceRequested: settingsSheet.openPlugins()

                Behavior on opacity {
                    NumberAnimation {
                        duration: root.mainTabDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
            }

            Loader {
                id: discoverLoader
                anchors.fill: parent
                // Keep alive after first open so tab switches aren't a cold CatalogPage create.
                property bool keepAlive: false
                active: mainPages.pageIndex === 1 || keepAlive
                asynchronous: true
                visible: status === Loader.Ready
                opacity: mainPages.pageIndex === 1 ? 1 : 0
                onLoaded: keepAlive = true
                sourceComponent: Component {
                    CatalogPage {
                        anchors.fill: parent
                        browseOnly: false
                        peekLeftEdge: navRail.width
                        enabled: mainPages.pageIndex === 1
                        onOpenGame: function (id) { root.openGameDetails(id, true) }
                        onOpenSettings: settingsSheet.openSettings()
                        onAddSourceRequested: settingsSheet.openPlugins()
                        onOpenFullCatalog: function (query) {
                            browseAllMode = false
                            root.openCatalogWithQuery(query)
                        }
                    }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: root.mainTabDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
            }

            Loader {
                id: catalogBrowseLoader
                anchors.fill: parent
                property bool keepAlive: false
                active: mainPages.pageIndex === 2 || keepAlive
                asynchronous: true
                visible: status === Loader.Ready
                opacity: mainPages.pageIndex === 2 ? 1 : 0
                onLoaded: keepAlive = true
                sourceComponent: Component {
                    CatalogPage {
                        anchors.fill: parent
                        browseOnly: true
                        peekLeftEdge: navRail.width
                        enabled: mainPages.pageIndex === 2
                        onOpenGame: function (id) { root.openGameDetails(id, true) }
                        onOpenSettings: settingsSheet.openSettings()
                        onAddSourceRequested: settingsSheet.openPlugins()
                    }
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: root.mainTabDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
            }

            BookmarksPage {
                anchors.fill: parent
                opacity: mainPages.pageIndex === 4 ? 1 : 0
                enabled: mainPages.pageIndex === 4 && opacity > 0.99
                onOpenGame: function (id) { root.openGameDetails(id, true) }

                Behavior on opacity {
                    NumberAnimation {
                        duration: root.mainTabDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
            }

            DownloadsPage {
                anchors.fill: parent
                opacity: mainPages.pageIndex === 5 ? 1 : 0
                enabled: mainPages.pageIndex === 5 && opacity > 0.99
                onOpenGame: function (id) { root.openGameDetails(id, false) }

                Behavior on opacity {
                    NumberAnimation {
                        duration: root.mainTabDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
            }

            FriendsPage {
                anchors.fill: parent
                opacity: mainPages.pageIndex === 3 ? 1 : 0
                enabled: mainPages.pageIndex === 3 && opacity > 0.99
                onOpenGame: function (id) { root.openGameDetails(id, true) }
                onOpenSettings: settingsSheet.openFriends()

                Behavior on opacity {
                    NumberAnimation {
                        duration: root.mainTabDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
            }
        }
    }

    Component {
        id: detailsPageComponent
        GameDetailsPage {
            transformOrigin: Item.Center
            onBackRequested: root.closeGameDetails()
            onOpenSourcePicker: function (entryId, title) {
                installSourceSheet.openForEntry(entryId, title)
            }
            onOpenAddonPicker: function (entryId, title) {
                installAddonSheet.openForEntry(entryId, title)
            }
            onOpenInstallPicker: function (entryId, title, selectedAddonIds, sourceId) {
                const catalogAddons = Core.catalog.addonsFor(entryId)
                const fromPicker = (selectedAddonIds && selectedAddonIds.length > 0)
                                   || (catalogAddons && catalogAddons.length > 0)
                installLocationSheet.openForEntry(entryId, title, selectedAddonIds, fromPicker,
                                                  sourceId || "")
            }
            onOpenSteamidraTrust: steamidraTrustSheet.openTrust()
            onOpenSourcesRequested: settingsSheet.openSources()
            onProtonRequired: protonRequiredDialog.open()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        AppTitleBar {
            visible: root.customTitleBar
            Layout.fillWidth: true
            window: root
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            AppRail {
                id: navRail
                Layout.fillHeight: true
                model: root.navModel
                currentIndex: root.pageIndex
                downloadBadge: root.downloadBadge
                onActivated: function (index) { root.goToPage(index) }
                onSettingsRequested: settingsSheet.openSettings()
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: MD.Token.spacing.small
                Layout.rightMargin: MD.Token.spacing.small
                Layout.bottomMargin: MD.Token.spacing.small
                spacing: 0

                MD.Pane {
                    id: mainPane
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    padding: 0
                    radius: MD.Token.shape.corner.extra_large
                    corners: MD.Util.corners(radius)
                    backgroundColor: MD.Token.color.surface

                    Item {
                        id: mainPaneClip
                        anchors.fill: parent
                        clip: true

                        layer.enabled: true
                        layer.effect: MD.RoundClip {
                            corners: mainPane.corners
                            size: Qt.vector2d(mainPaneClip.width, mainPaneClip.height)
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Loader {
                                id: runningGameBarLoader
                                Layout.fillWidth: true
                                Layout.fillHeight: false
                                // ColumnLayout otherwise stretches the Loader and leaves a huge
                                // empty gap under the running-game chip.
                                readonly property real barHeight: active && item ? item.implicitHeight : 0
                                Layout.preferredHeight: barHeight
                                Layout.maximumHeight: barHeight
                                Layout.leftMargin: MD.Token.spacing.medium
                                Layout.rightMargin: MD.Token.spacing.medium
                                Layout.topMargin: active ? MD.Token.spacing.medium : 0
                                Layout.bottomMargin: active ? MD.Token.spacing.small : 0
                                // Library home hero already shows "Playing now" + Stop.
                                active: Core.gameRunning
                                        && !(root.pageIndex === 0 && !root.detailsOpen)
                                visible: active
                                sourceComponent: RunningGameBar {
                                    gameId: Core.runningGameId
                                    title: Core.runningGameTitle
                                    coverUrl: Core.runningGameCoverUrl
                                }
                            }

                            PageNavigator {
                                id: pageStack
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                initialItem: mainPagesComponent
                            }
                        }
                    }
                }
            }
        }
    }

    WindowResizeEdges {
        anchors.fill: parent
        visible: root.customTitleBar
        window: root
        z: 1000
    }

    SettingsSheet {
        id: settingsSheet
        anchors.fill: parent
    }

    OnboardingSheet {
        id: onboardingSheet
        anchors.fill: parent
        onFinished: {
            if (Core.hasPendingCrashReport())
                Qt.callLater(function () { crashReportDialog.open() })
        }
    }

    InstallLocationSheet {
        id: installLocationSheet
        anchors.fill: parent
        installEntry: function (entryId, libraryId, addonIds, sourceId) {
            root.beginCatalogInstall(entryId, libraryId, addonIds, sourceId)
        }
        onBackToAddons: function (entryId, title, selectedAddonIds) {
            installAddonSheet.openForEntry(entryId, title)
        }
    }

    InstallSourceSheet {
        id: installSourceSheet
        anchors.fill: parent
        onSourceChosen: function (entryId, offerEntryId, sourceId, title) {
            const page = pageStack.currentItem
            if (page && typeof page.afterSourceSelected === "function") {
                page.afterSourceSelected(offerEntryId, sourceId)
                return
            }
            // Fallback when details page is not current (should be rare).
            if (Core.needsInstallLocationChoice())
                installLocationSheet.openForEntry(offerEntryId || entryId, title, [], false,
                                                  sourceId || "")
            else
                Core.installCatalogEntryFromSource(entryId, sourceId, "", [])
        }
    }

    InstallAddonSelectionSheet {
        id: installAddonSheet
        anchors.fill: parent
        onConfirmed: function (entryId, title, selectedAddonIds) {
            const page = pageStack.currentItem
            if (page && typeof page.afterAddonsSelected === "function")
                page.afterAddonsSelected(selectedAddonIds)
            else if (Core.needsInstallLocationChoice()) {
                const catalogAddons = Core.catalog.addonsFor(entryId)
                const fromPicker = (selectedAddonIds && selectedAddonIds.length > 0)
                                   || (catalogAddons && catalogAddons.length > 0)
                const details = Core.entryDetails(entryId)
                installLocationSheet.openForEntry(entryId, title, selectedAddonIds, fromPicker,
                                                  details.sourceId || "")
            } else
                root.beginCatalogInstall(entryId, "", selectedAddonIds)
        }
    }

    SteamidraTrustSheet {
        id: steamidraTrustSheet
        anchors.fill: parent
    }

    ProtonRequiredDialog {
        id: protonRequiredDialog
        onOpenLaunchSettings: settingsSheet.openLaunch()
    }

    CrashReportDialog {
        id: crashReportDialog
    }

    AppUpdateSheet {
        id: appUpdateSheet
        anchors.fill: parent
        z: 1900
    }

    AppUpdateProgressOverlay {
        anchors.fill: parent
    }

    PluginInstallOverlay {
        anchors.fill: parent
    }

    AppSnackbar {
        id: snackbar
        anchors.fill: parent
        anchors.leftMargin: 88
        z: 3200
    }

}
