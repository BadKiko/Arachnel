pragma Singleton

import QtQuick

// Long / shared UI copy. English source in qsTr(); Russian in translations/arachnel_ru.ts.
QtObject {
    readonly property string helpCatalogIntro: qsTr("Install a plugin in Settings → Plugins to browse games, or add a catalog.")

    readonly property string helpHydraCatalogBody: qsTr("Paste a catalog link in Settings → Hydra catalogs. Games show up in Catalog.")

    readonly property string helpCatalogBody: qsTr("Pick a game in Catalog to start a download.")

    readonly property string helpLibraryBody: qsTr("After download and install, the game appears here - launch, updates, and details.")

    readonly property string settingsSourcesConnectHint: qsTr("Paste a JSON catalog URL from Hydra or another list.")

    readonly property string settingsSourcesAddHint: qsTr("Add a catalog and paste the JSON link.")

    readonly property string settingsSourceFormDesc: qsTr("Paste a catalog JSON URL. Arachnel loads the game list from that link.")

    readonly property string settingsWeblateHint: qsTr("Missing your language? Help translate Arachnel on <a href=\"%1\">Weblate</a>.")

    readonly property string settingsPluginsInstallHint: qsTr("Use Install plugin below and pick a .arach file.")

    readonly property string settingsPluginsDesc: qsTr("Arachnel has no games until you install a plugin. Each plugin is a source: it fills Catalog and handles download, install, and Play.")

    readonly property string libraryEmptySubtitle: qsTr("Your library is empty. Install a plugin, pick a game in Catalog, and it will appear here.")

    readonly property string libraryStep1Body: qsTr("Install a plugin in Settings → Plugins.")

    readonly property string libraryStep2Body: qsTr("Pick a game in Catalog and start the download.")

    readonly property string libraryStep3Body: qsTr("Installed games live here: launch, updates, and details.")

    readonly property string gameInstallTorrentHint: qsTr("Download finished. Click Install to set up the game.")

    readonly property string steamidraTrustMarkdown: qsTr("### Where do the files come from?\n\nGame **chunks** are downloaded from the **Valve Steam CDN** - the same CDN Steam uses for depot files.\n\n### What is Online Fix?\n\nMany multiplayer titles need an **Online Fix** (Steam API shim). The Steam plugin can include it so the game runs and goes online without a Store purchase license check.\n\n### What is *not* from Valve?\n\n- Depot **keys** and **manifests** come from the plugin relay (not the Steam Store).\n- This is **not** the same as buying the game on Steam.\n- Arachnel does **not** claim antivirus clearance or Valve endorsement.")

    readonly property string catalogPipelineDesc: qsTr("Browse games from your catalogs and sources.")

    readonly property string catalogConnectHint: qsTr("Install a plugin in Settings → Plugins, or add a catalog.")

    readonly property string catalogEnableChipsHint: qsTr("Turn on one or more sources above - or leave them all off.")

    readonly property string storageLibrariesDesc: qsTr("Libraries on disk, like Steam. You can add other drives.")

    readonly property string addonsSelectionHint: qsTr("Add-ons are available for \"%1\" - choose what to download with the game.")

    readonly property string downloadsEmptyHint: qsTr("Start installing from the catalog - progress will appear here.")

    readonly property string favoritesEmptyHint: qsTr("Save games from the catalog - download them here later.")

    readonly property string gameNotFoundHint: qsTr("It may be missing from your sources, or a plugin is outdated. Check Sources in Settings.")

    readonly property string gameDeleteWarning: qsTr("Game files will be deleted from disk. This cannot be undone.")
}
