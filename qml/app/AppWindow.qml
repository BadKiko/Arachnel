import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.ApplicationWindow {
    id: root

    visible: true
    width: 1440
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
        if (pageStack.currentItem && pageStack.currentItem.pageIndex !== undefined)
            pageStack.currentItem.pageIndex = index
    }

    function openGameDetails(gameId, fromCatalog) {
        root.detailsGameId = gameId
        root.detailsFromCatalog = !!fromCatalog
        pageStack.navigatePush(detailsPageComponent, {
                                   "gameId": gameId,
                                   "fromCatalog": !!fromCatalog
                               })
    }

    function closeGameDetails() {
        if (pageStack.canPop)
            pageStack.navigatePop()
        root.detailsGameId = ""
    }

    function beginCatalogInstall(entryId, libraryId, addonIds) {
        if (Core.needsProtonOnPlatform() && !Core.protonReady) {
            protonRequiredDialog.open()
            return
        }
        Core.installCatalogEntry(entryId, libraryId || "", addonIds || [])
    }

    Component.onCompleted: {
        Appearance.apply()
        if (Qt.platform.os === "linux")
            Core.refreshProtonLatestRelease()
        if (!Core.settings.onboardingCompleted)
            Qt.callLater(function () { onboardingSheet.openWizard() })
        else if (Core.hasPendingCrashReport())
            Qt.callLater(function () { crashReportDialog.open() })
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
            name: qsTr("Catalog"),
            icon: MD.Token.icon.storefront
        },
        {
            name: qsTr("Downloads"),
            icon: MD.Token.icon.downloading,
            navIndex: 2
        }
    ]

    Component {
        id: mainPagesComponent
        Item {
            id: mainPages

            property int pageIndex: root.pageIndex

            transformOrigin: Item.Center

            // Background comes from MD.Pane in AppWindow — no fill rect here (it hid bottom corners).
            // Soft crossfade + light bounce on the active tab.
            LibraryPage {
                anchors.fill: parent
                opacity: mainPages.pageIndex === 0 ? 1 : 0
                scale: mainPages.pageIndex === 0 ? 1 : 0.97
                enabled: mainPages.pageIndex === 0 && opacity > 0.99
                transformOrigin: Item.Center
                onOpenGame: function (id) { root.openGameDetails(id, false) }
                onOpenCatalog: root.goToPage(1)
                onOpenDownloads: root.goToPage(2)
                onOpenSettings: settingsSheet.openSettings()
                onAddSourceRequested: settingsSheet.openPlugins()

                Behavior on opacity {
                    NumberAnimation {
                        duration: pageStack.enterDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
                Behavior on scale {
                    NumberAnimation {
                        duration: pageStack.enterDuration
                        easing.type: Easing.OutBack
                        easing.overshoot: 1.2
                    }
                }
            }

            CatalogPage {
                anchors.fill: parent
                opacity: mainPages.pageIndex === 1 ? 1 : 0
                scale: mainPages.pageIndex === 1 ? 1 : 0.97
                enabled: mainPages.pageIndex === 1
                transformOrigin: Item.Center
                onOpenGame: function (id) { root.openGameDetails(id, true) }
                onOpenSettings: settingsSheet.openSettings()
                onAddSourceRequested: settingsSheet.openPlugins()

                Behavior on opacity {
                    NumberAnimation {
                        duration: pageStack.enterDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
                Behavior on scale {
                    NumberAnimation {
                        duration: pageStack.enterDuration
                        easing.type: Easing.OutBack
                        easing.overshoot: 1.2
                    }
                }
            }

            DownloadsPage {
                anchors.fill: parent
                opacity: mainPages.pageIndex === 2 ? 1 : 0
                scale: mainPages.pageIndex === 2 ? 1 : 0.97
                enabled: mainPages.pageIndex === 2 && opacity > 0.99
                transformOrigin: Item.Center
                onOpenGame: function (id) { root.openGameDetails(id, false) }

                Behavior on opacity {
                    NumberAnimation {
                        duration: pageStack.enterDuration
                        easing: MD.Token.easing.emphasized_decelerate
                    }
                }
                Behavior on scale {
                    NumberAnimation {
                        duration: pageStack.enterDuration
                        easing.type: Easing.OutBack
                        easing.overshoot: 1.2
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
            onOpenAddonPicker: function (entryId, title) {
                installAddonSheet.openForEntry(entryId, title)
            }
            onOpenInstallPicker: function (entryId, title, selectedAddonIds) {
                installLocationSheet.openForEntry(entryId, title, selectedAddonIds)
            }
            onOpenSteamidraTrust: steamidraTrustSheet.openTrust()
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
                                active: Core.gameRunning
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
        installEntry: function (entryId, libraryId, addonIds) {
            root.beginCatalogInstall(entryId, libraryId, addonIds)
        }
    }

    InstallAddonSelectionSheet {
        id: installAddonSheet
        anchors.fill: parent
        onConfirmed: function (entryId, title, selectedAddonIds) {
            const page = pageStack.currentItem
            if (page && typeof page.afterAddonsSelected === "function")
                page.afterAddonsSelected(selectedAddonIds)
            else if (Core.needsInstallLocationChoice())
                installLocationSheet.openForEntry(entryId, title, selectedAddonIds)
            else
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
