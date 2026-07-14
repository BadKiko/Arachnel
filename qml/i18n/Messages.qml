pragma Singleton

import QtQuick

// Short qsTrId keys for Weblate (e.g. "help.catalog_intro" instead of a full English sentence).
QtObject {
    //% "Two ways to fill the catalog: Hydra catalogs (games.json) for migrating from Hydra; plugins (.arach) for the full cycle: catalog, install, launch, and add-ons."
    readonly property string helpCatalogIntro: qsTrId("help.catalog_intro")

    //% "A games.json feed by URL — the same format as Hydra Launcher. Add it under Settings → Hydra catalogs; games appear in Catalog."
    readonly property string helpHydraCatalogBody: qsTrId("help.hydra_catalog_body")

    //% "Games from enabled catalogs appear in Catalog. Downloads use torrent magnets from JSON."
    readonly property string helpCatalogBody: qsTrId("help.catalog_body")

    //% "After download and install, the game appears here — launch, updates, and details."
    readonly property string helpLibraryBody: qsTrId("help.library_body")

    //% "Connect catalogs in Hydra Launcher format (games.json). Handy when migrating from Hydra — same game links, torrent downloads. A source plugin is required to install and launch."
    readonly property string settingsSourcesConnectHint: qsTrId("settings.sources.connect_hint")

    //% "Click Add catalog and paste your games.json URL — like Hydra, or a public community feed."
    readonly property string settingsSourcesAddHint: qsTrId("settings.sources.add_hint")

    //% "Hydra catalog — a games.json JSON feed by URL. Arachnel pulls the game list and magnet links like Hydra Launcher. Install and launch via a source plugin (e.g. FreeTP)."
    readonly property string settingsSourceFormDesc: qsTrId("settings.source_form.desc")

    //% "Missing your language? Help translate Arachnel on <a href=\"%1\">Weblate</a> or send a pull request with translations/*.ts files."
    readonly property string settingsWeblateHint: qsTrId("settings.appearance.weblate_hint")

    //% "Install the .arach package using the button below.\n\nBuild the plugin from the arachnel-plugin-freetp repository (dist/freetp.arach)."
    readonly property string settingsPluginsInstallHint: qsTrId("settings.plugins.install_hint")

    //% "Plugins are sources with catalog, install, and launch. Package: .arach file (ZIP with plugin.json and libraries)."
    readonly property string settingsPluginsDesc: qsTrId("settings.plugins.desc")

    //% "Your library is empty. Install a source plugin, pick a game in the catalog, and it will show up here."
    readonly property string libraryEmptySubtitle: qsTrId("library.empty.subtitle")

    //% "Install a source plugin (FreeTP, etc.) under Settings → Plugins."
    readonly property string libraryStep1Body: qsTrId("library.step1.body")

    //% "Pick a game and start installation — the torrent downloads automatically."
    readonly property string libraryStep2Body: qsTrId("library.step2.body")

    //% "Installed games live here: launch, updates, and details."
    readonly property string libraryStep3Body: qsTrId("library.step3.body")

    //% "Torrent downloaded. Click Install for the source plugin to extract or install the game."
    readonly property string gameInstallTorrentHint: qsTrId("game.install.torrent_hint")

    //% "The source defines how installation works — each plugin has its own pipeline."
    readonly property string catalogPipelineDesc: qsTrId("catalog.pipeline_desc")

    //% "Connect a Hydra catalog in Settings — or install a source plugin."
    readonly property string catalogConnectHint: qsTrId("catalog.connect_hint")

    //% "Enable one or more source chips — or leave them all off."
    readonly property string catalogEnableChipsHint: qsTrId("catalog.enable_chips_hint")

    //% "Libraries on disks — like Steam. Default is C:; you can add other drives."
    readonly property string storageLibrariesDesc: qsTrId("storage.libraries_desc")

    //% "Add-ons are available for \"%1\" — choose what to download with the game."
    readonly property string addonsSelectionHint: qsTrId("addons.selection_hint")

    //% "Start installing from the catalog — progress will appear here."
    readonly property string downloadsEmptyHint: qsTrId("downloads.empty_hint")

    //% "Game files will be deleted from disk. This cannot be undone."
    readonly property string gameDeleteWarning: qsTrId("game.delete_warning")
}
