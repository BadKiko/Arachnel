pragma Singleton

import QtQuick

// Short qsTrId keys for Weblate (e.g. "help.catalog_intro" instead of a full English sentence).
QtObject {
    //% "Add a catalog to browse games, or install a plugin for download, install, and play."
    readonly property string helpCatalogIntro: qsTrId("help.catalog_intro")

    //% "Paste a catalog link in Settings → Hydra catalogs. Games show up in Catalog."
    readonly property string helpHydraCatalogBody: qsTrId("help.hydra_catalog_body")

    //% "Pick a game in Catalog to start a download."
    readonly property string helpCatalogBody: qsTrId("help.catalog_body")

    //% "After download and install, the game appears here — launch, updates, and details."
    readonly property string helpLibraryBody: qsTrId("help.library_body")

    //% "Add a catalog URL from Hydra or another community list. Install a plugin (e.g. FreeTP) to install and play."
    readonly property string settingsSourcesConnectHint: qsTrId("settings.sources.connect_hint")

    //% "Tap Add catalog and paste the catalog link."
    readonly property string settingsSourcesAddHint: qsTrId("settings.sources.add_hint")

    //% "Paste a catalog URL. Arachnel loads the game list; a plugin handles install and launch."
    readonly property string settingsSourceFormDesc: qsTrId("settings.source_form.desc")

    //% "Missing your language? Help translate Arachnel on <a href=\"%1\">Weblate</a>."
    readonly property string settingsWeblateHint: qsTrId("settings.appearance.weblate_hint")

    //% "Use Install plugin below and pick a plugin file (e.g. FreeTP)."
    readonly property string settingsPluginsInstallHint: qsTrId("settings.plugins.install_hint")

    //% "Plugins add catalogs and handle install, updates, and launch (e.g. FreeTP)."
    readonly property string settingsPluginsDesc: qsTrId("settings.plugins.desc")

    //% "Your library is empty. Install a plugin, pick a game in Catalog, and it will appear here."
    readonly property string libraryEmptySubtitle: qsTrId("library.empty.subtitle")

    //% "Install a plugin (e.g. FreeTP) in Settings → Plugins."
    readonly property string libraryStep1Body: qsTrId("library.step1.body")

    //% "Pick a game in Catalog and start the download."
    readonly property string libraryStep2Body: qsTrId("library.step2.body")

    //% "Installed games live here: launch, updates, and details."
    readonly property string libraryStep3Body: qsTrId("library.step3.body")

    //% "Download finished. Click Install to set up the game."
    readonly property string gameInstallTorrentHint: qsTrId("game.install.torrent_hint")

    //% "### Where do the files come from?\n\nGame **chunks** are downloaded from the **Valve Steam CDN** — the same content delivery network Steam itself uses for depot files.\n\n### What is Online Fix?\n\nMany multiplayer titles need an **Online Fix** (Steam API shim). The Steam plugin can include it so the game runs and goes online without a Store purchase license check.\n\n### What is *not* from Valve?\n\n- Depot **keys** and **manifests** come from the plugin relay (not the Steam Store).\n- This is **not** the same as buying the game on Steam.\n- Arachnel does **not** claim antivirus clearance or Valve endorsement."
    readonly property string steamidraTrustMarkdown: qsTrId("steamidra.trust.markdown")

    //% "Browse games from your catalogs and sources."
    readonly property string catalogPipelineDesc: qsTrId("catalog.pipeline_desc")

    //% "Add a catalog or install a plugin in Settings."
    readonly property string catalogConnectHint: qsTrId("catalog.connect_hint")

    //% "Turn on one or more sources above — or leave them all off."
    readonly property string catalogEnableChipsHint: qsTrId("catalog.enable_chips_hint")

    //% "Libraries on disks — like Steam. You can add other drives."
    readonly property string storageLibrariesDesc: qsTrId("storage.libraries_desc")

    //% "Add-ons are available for \"%1\" — choose what to download with the game."
    readonly property string addonsSelectionHint: qsTrId("addons.selection_hint")

    //% "Start installing from the catalog — progress will appear here."
    readonly property string downloadsEmptyHint: qsTrId("downloads.empty_hint")

    //% "Game files will be deleted from disk. This cannot be undone."
    readonly property string gameDeleteWarning: qsTrId("game.delete_warning")
}
