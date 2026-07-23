<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US">
<context>
    <name />
    <message id="help.catalog_intro">
        
        <location filename="../qml/i18n/Messages.qml" line="8" />
        <source>Add a catalog to browse games, or install a plugin for download, install, and play.</source>
        <oldsource>Two ways to fill the catalog: Hydra catalogs (games.json) for migrating from Hydra; plugins (.arach) for the full cycle: catalog, install, launch, and add-ons.</oldsource>
        </message>
    <message id="help.hydra_catalog_body">
        
        <location filename="../qml/i18n/Messages.qml" line="11" />
        <source>Paste a catalog link in Settings → Hydra catalogs. Games show up in Catalog.</source>
        <oldsource>A games.json feed by URL — the same format as Hydra Launcher. Add it under Settings → Hydra catalogs; games appear in Catalog.</oldsource>
        </message>
    <message id="help.catalog_body">
        
        <location filename="../qml/i18n/Messages.qml" line="14" />
        <source>Pick a game in Catalog to start a download.</source>
        <oldsource>Games from enabled catalogs appear in Catalog. Downloads use torrent magnets from JSON.</oldsource>
        </message>
    <message id="help.library_body">
        
        <location filename="../qml/i18n/Messages.qml" line="17" />
        <source>After download and install, the game appears here — launch, updates, and details.</source>
        </message>
    <message id="settings.sources.connect_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="20" />
        <source>Add a catalog URL from Hydra or another community list. Install a plugin (e.g. FreeTP) to install and play.</source>
        <oldsource>Connect catalogs in Hydra Launcher format (games.json). Handy when migrating from Hydra — same game links, torrent downloads. A source plugin is required to install and launch.</oldsource>
        </message>
    <message id="settings.sources.add_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="23" />
        <source>Tap Add catalog and paste the catalog link.</source>
        <oldsource>Click Add catalog and paste your games.json URL — like Hydra, or a public community feed.</oldsource>
        </message>
    <message id="settings.source_form.desc">
        
        <location filename="../qml/i18n/Messages.qml" line="26" />
        <source>Paste a catalog URL. Arachnel loads the game list; a plugin handles install and launch.</source>
        <oldsource>Hydra catalog — a games.json JSON feed by URL. Arachnel pulls the game list and magnet links like Hydra Launcher. Install and launch via a source plugin (e.g. FreeTP).</oldsource>
        </message>
    <message id="settings.appearance.weblate_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="29" />
        <source>Missing your language? Help translate Arachnel on &lt;a href="%1"&gt;Weblate&lt;/a&gt;.</source>
        <oldsource>Missing your language? Help translate Arachnel on &lt;a href="%1"&gt;Weblate&lt;/a&gt; or send a pull request with translations/*.ts files.</oldsource>
        </message>
    <message id="settings.plugins.install_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="32" />
        <source>Use Install plugin below and pick a plugin file (e.g. FreeTP).</source>
        <oldsource>Install the .arach package using the button below.

Build the plugin from the arachnel-plugin-freetp repository (dist/freetp.arach).</oldsource>
        </message>
    <message id="settings.plugins.desc">
        
        <location filename="../qml/i18n/Messages.qml" line="35" />
        <source>Plugins add catalogs and handle install, updates, and launch (e.g. FreeTP).</source>
        <oldsource>Plugins are sources with catalog, install, and launch. Package: .arach file (ZIP with plugin.json and libraries).</oldsource>
        </message>
    <message id="library.empty.subtitle">
        
        <location filename="../qml/i18n/Messages.qml" line="38" />
        <source>Your library is empty. Install a plugin, pick a game in Catalog, and it will appear here.</source>
        <oldsource>Your library is empty. Install a source plugin, pick a game in the catalog, and it will show up here.</oldsource>
        </message>
    <message id="library.step1.body">
        
        <location filename="../qml/i18n/Messages.qml" line="41" />
        <source>Install a plugin (e.g. FreeTP) in Settings → Plugins.</source>
        <oldsource>Install a source plugin (FreeTP, etc.) under Settings → Plugins.</oldsource>
        </message>
    <message id="library.step2.body">
        
        <location filename="../qml/i18n/Messages.qml" line="44" />
        <source>Pick a game in Catalog and start the download.</source>
        <oldsource>Pick a game and start installation — the torrent downloads automatically.</oldsource>
        </message>
    <message id="library.step3.body">
        
        <location filename="../qml/i18n/Messages.qml" line="47" />
        <source>Installed games live here: launch, updates, and details.</source>
        </message>
    <message id="game.install.torrent_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="50" />
        <source>Download finished. Click Install to set up the game.</source>
        <oldsource>Torrent downloaded. Click Install for the source plugin to extract or install the game.</oldsource>
        </message>
    <message id="steamidra.trust.markdown">
        
        <location filename="../qml/i18n/Messages.qml" line="53" />
        <source>### Where do the files come from?

Game **chunks** are downloaded from the **Valve Steam CDN** — the same content delivery network Steam itself uses for depot files.

### What is Online Fix?

Many multiplayer titles need an **Online Fix** (Steam API shim). The Steam plugin can include it so the game runs and goes online without a Store purchase license check.

### What is *not* from Valve?

- Depot **keys** and **manifests** come from the plugin relay (not the Steam Store).
- This is **not** the same as buying the game on Steam.
- Arachnel does **not** claim antivirus clearance or Valve endorsement.</source>
        </message>
    <message id="catalog.pipeline_desc">
        
        <location filename="../qml/i18n/Messages.qml" line="56" />
        <source>Browse games from your catalogs and sources.</source>
        <oldsource>The source defines how installation works — each plugin has its own pipeline.</oldsource>
        </message>
    <message id="catalog.connect_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="59" />
        <source>Add a catalog or install a plugin in Settings.</source>
        <oldsource>Connect a Hydra catalog in Settings — or install a source plugin.</oldsource>
        </message>
    <message id="catalog.enable_chips_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="62" />
        <source>Turn on one or more sources above — or leave them all off.</source>
        <oldsource>Enable one or more source chips — or leave them all off.</oldsource>
        </message>
    <message id="storage.libraries_desc">
        
        <location filename="../qml/i18n/Messages.qml" line="65" />
        <source>Libraries on disks — like Steam. You can add other drives.</source>
        <oldsource>Libraries on disks — like Steam. Default is C:; you can add other drives.</oldsource>
        </message>
    <message id="addons.selection_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="68" />
        <source>Add-ons are available for "%1" — choose what to download with the game.</source>
        </message>
    <message id="downloads.empty_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="71" />
        <source>Start installing from the catalog — progress will appear here.</source>
        </message>
    <message id="game.delete_warning">
        
        <location filename="../qml/i18n/Messages.qml" line="74" />
        <source>Game files will be deleted from disk. This cannot be undone.</source>
        </message>

</context>
<context>
    <name>AppUpdateProgressOverlay</name>
    <message>
        
        <location filename="../qml/app/AppUpdateProgressOverlay.qml" line="42" />
        <source>Downloading Arachnel update…</source>
        </message>
    <message>
        
        <location filename="../qml/app/AppUpdateProgressOverlay.qml" line="48" />
        <source>Please wait. The installer will open automatically.</source>
        </message>
    <message>
        
        <location filename="../qml/app/AppUpdateProgressOverlay.qml" line="80" />
        <source>Starting…</source>
        </message>

</context>
<context>
    <name>AppUpdateSheet</name>
    <message>
        
        <location filename="../qml/components/AppUpdateSheet.qml" line="31" />
        <source>Update available</source>
        </message>
    <message>
        
        <location filename="../qml/components/AppUpdateSheet.qml" line="39" />
        <source>Arachnel %1 is ready to install. Update now to get the latest fixes and features.</source>
        </message>
    <message>
        
        <location filename="../qml/components/AppUpdateSheet.qml" line="52" />
        <source>Current version: %1</source>
        </message>
    <message>
        
        <location filename="../qml/components/AppUpdateSheet.qml" line="67" />
        <source>Later</source>
        </message>
    <message>
        
        <location filename="../qml/components/AppUpdateSheet.qml" line="75" />
        <source>Release page</source>
        </message>
    <message>
        
        <location filename="../qml/components/AppUpdateSheet.qml" line="87" />
        <source>Downloading… %1%</source>
        </message>
    <message>
        
        <location filename="../qml/components/AppUpdateSheet.qml" line="88" />
        <source>Update now</source>
        </message>

</context>
<context>
    <name>AppWindow</name>
    <message>
        
        <location filename="../qml/app/AppWindow.qml" line="16" />
        <source>Arachnel</source>
        </message>
    <message>
        
        <location filename="../qml/app/AppWindow.qml" line="111" />
        <source>Library</source>
        </message>
    <message>
        
        <location filename="../qml/app/AppWindow.qml" line="115" />
        <source>Catalog</source>
        </message>
    <message>
        
        <location filename="../qml/app/AppWindow.qml" line="119" />
        <source>Downloads</source>
        </message>

</context>
<context>
    <name>CatalogCompactBar</name>
    <message>
        
        <location filename="../qml/components/CatalogCompactBar.qml" line="45" />
        <source>Catalog</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogCompactBar.qml" line="54" />
        <source>Loading…</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogCompactBar.qml" line="55" />
        <source>Found: %1</source>
        </message>

</context>
<context>
    <name>CatalogEmptyResults</name>
    <message>
        
        <location filename="../qml/components/CatalogEmptyResults.qml" line="53" />
        <source>Select sources</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogEmptyResults.qml" line="54" />
        <source>Nothing found</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogEmptyResults.qml" line="62" />
        <source>Try another search or refresh the catalog.</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogEmptyResults.qml" line="70" />
        <source>Refresh</source>
        </message>

</context>
<context>
    <name>CatalogFilterSheet</name>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="26" />
        <source>All</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="27" />
        <source>Portable</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="28" />
        <source>Installer</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="29" />
        <source>Online fix</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="33" />
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="40" />
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="48" />
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="312" />
        <source>Any</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="34" />
        <source>Single-player</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="35" />
        <source>Co-op</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="36" />
        <source>Multiplayer</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="41" />
        <source>&lt; 1 GB</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="42" />
        <source>1–5 GB</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="43" />
        <source>5–20 GB</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="44" />
        <source>20+ GB</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="49" />
        <source>Last 7 days</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="50" />
        <source>Last 30 days</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="51" />
        <source>Last 90 days</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="52" />
        <source>Last year</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="93" />
        <source>Sort &amp; filters</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="117" />
        <source>Sort</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="148" />
        <source>Type</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="179" />
        <source>Players</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="210" />
        <source>Size</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="241" />
        <source>Added</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="272" />
        <source>Extras</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="283" />
        <source>Has add-ons</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="302" />
        <source>Genre</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="351" />
        <source>Clear all</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogFilterSheet.qml" line="358" />
        <source>Apply</source>
        </message>

</context>
<context>
    <name>CatalogIntroHeader</name>
    <message>
        
        <location filename="../qml/components/CatalogIntroHeader.qml" line="14" />
        <source>Catalog</source>
        </message>

</context>
<context>
    <name>CatalogNoSourcesPanel</name>
    <message>
        
        <location filename="../qml/components/CatalogNoSourcesPanel.qml" line="31" />
        <source>No games</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogNoSourcesPanel.qml" line="49" />
        <source>Add catalog</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogNoSourcesPanel.qml" line="56" />
        <source>Settings</source>
        </message>

</context>
<context>
    <name>CatalogPage</name>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="40" />
        <source>Newest first</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="41" />
        <source>Oldest first</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="42" />
        <source>Title A–Z</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="43" />
        <source>Title Z–A</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="44" />
        <source>Portable first</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="45" />
        <source>Non-portable first</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="46" />
        <source>Largest first</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="47" />
        <source>Smallest first</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="53" />
        <source>Installer</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="54" />
        <source>Online fix</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="57" />
        <source>Any size</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="58" />
        <source>&lt; 1 GB</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="59" />
        <source>1–5 GB</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="60" />
        <source>5–20 GB</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="61" />
        <source>20+ GB</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="64" />
        <source>Any time</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="65" />
        <source>Last 7 days</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="66" />
        <source>Last 30 days</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="67" />
        <source>Last 90 days</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="68" />
        <source>Last year</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="51" />
        <source>All</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="52" />
        <source>Portable</source>
        </message>

</context>
<context>
    <name>CatalogScrollHeader</name>
    <message>
        
        <location filename="../qml/components/CatalogScrollHeader.qml" line="34" />
        <source>Loading catalog…</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogScrollHeader.qml" line="35" />
        <source>Found: %1</source>
        </message>

</context>
<context>
    <name>CatalogSourceChips</name>
    <message>
        
        <location filename="../qml/components/CatalogSourceChips.qml" line="27" />
        <source>%1 · %2</source>
        </message>

</context>
<context>
    <name>CatalogStickyToolbar</name>
    <message>
        
        <location filename="../qml/components/CatalogStickyToolbar.qml" line="78" />
        <source>Type</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogStickyToolbar.qml" line="86" />
        <source>Size</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogStickyToolbar.qml" line="94" />
        <source>Added</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogStickyToolbar.qml" line="102" />
        <source>Has add-ons</source>
        </message>

</context>
<context>
    <name>Core</name>
    <message>
        
        <location filename="../src/core/catalog/catalog_types.cpp" line="84" />
        <source>Game</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_types.cpp" line="88" />
        <source>Add-on</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_types.cpp" line="90" />
        <source>Component</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_types.cpp" line="97" />
        <source>Direct</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_types.cpp" line="99" />
        <source>Torrent</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_kind.cpp" line="11" />
        
        <location filename="../src/core/catalog/catalog_types.cpp" line="101" />
        <source>Download</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_kind.cpp" line="11" />
        <source>Portable</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_kind.cpp" line="13" />
        <source>Installer</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_kind.cpp" line="15" />
        <source>Bundled fix</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_kind.cpp" line="17" />
        <source>Separate fix</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_kind.cpp" line="19" />
        <source>Unknown</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="45" />
        
        <location filename="../src/core/jobs/job_display.cpp" line="92" />
        
        <location filename="../src/core/jobs/job_display.cpp" line="97" />
        <source>Add-on %1 — %2</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="43" />
        
        <location filename="../src/core/jobs/job_display.cpp" line="174" />
        
        <location filename="../src/core/jobs/job_display.cpp" line="176" />
        <source>Install failed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="13" />
        <source>Download complete</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="14" />
        <source>Installation required</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="15" />
        <source>Downloading…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="16" />
        <source>Connecting…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="17" />
        <source>Installing…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="18" />
        <source>Installing add-on…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="19" />
        <source>Preparing…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="20" />
        <source>Preparing Steam…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="21" />
        <source>Downloading from Steam CDN…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="22" />
        <source>Getting game info…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="23" />
        <source>Finishing…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="24" />
        <source>This game is not available for download right now. Try another title.</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="25" />
        <source>Could not prepare this game for download. Try again later or pick another title.</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="27" />
        <source>Download failed. Try again or pick another game.</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="28" />
        <source>Steam blocked downloading game files (need a packaged manifest). Try another title, or set hubcapApiKey in plugin settings.</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="32" />
        <source>Resuming…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="36" />
        <source>Failed to start torrent</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="37" />
        <source>Failed to start HTTP download</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="39" />
        <source>Downloading %1</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="40" />
        <source>Installing %1</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="41" />
        <source>Updating %1</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="42" />
        
        <location filename="../src/core/jobs/job_display.cpp" line="179" />
        
        <location filename="../src/core/jobs/job_display.cpp" line="183" />
        <source>Installing (%1/%2)</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="44" />
        
        <location filename="../src/core/jobs/job_display.cpp" line="188" />
        
        <location filename="../src/core/jobs/job_display.cpp" line="190" />
        <source>Error: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_kind.cpp" line="13" />
        <source>Install</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_kind.cpp" line="15" />
        <source>Update</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_kind.cpp" line="17" />
        <source>Task</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_status.cpp" line="10" />
        <source>Queued</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_status.cpp" line="12" />
        <source>Starting</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_status.cpp" line="14" />
        <source>Checking</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_status.cpp" line="16" />
        <source>Metadata</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_status.cpp" line="18" />
        <source>Downloading</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_status.cpp" line="20" />
        <source>Installing</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_status.cpp" line="22" />
        <source>Seeding</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="33" />
        
        <location filename="../src/core/jobs/job_status.cpp" line="24" />
        <source>Paused</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="38" />
        
        <location filename="../src/core/jobs/job_status.cpp" line="26" />
        <source>Completed</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_status.cpp" line="28" />
        <source>Failed</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="34" />
        
        <location filename="../src/core/jobs/job_status.cpp" line="30" />
        <source>Cancelled</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="35" />
        
        <location filename="../src/core/jobs/job_status.cpp" line="37" />
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="275" />
        <source>Install failed</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_controller.cpp" line="50" />
        <source>Catalog error: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="361" />
        <source>Game not found for add-on</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="366" />
        <source>Add-on not found in catalog</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="348" />
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="377" />
        <source>Could not find game to install: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="413" />
        <source>Download error: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_session_service.cpp" line="38" />
        <source>Installation of %1 is already in progress</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_session_service.cpp" line="99" />
        <source>Install failed for %1: %2</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_session_service.cpp" line="131" />
        <source>Update installed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_session_service.cpp" line="132" />
        <source>Installed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_session_service_install.cpp" line="26" />
        <source>Add-on installation is already in progress</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_session_service_install.cpp" line="33" />
        <source>Install the game first</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_session_service_install.cpp" line="85" />
        <source>Add-on install failed for %1: %2</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_session_service_install.cpp" line="99" />
        <source>Add-on installed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="128" />
        <source>Preparing runtime environment…</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_1.cpp" line="101" />
        <source>Game not found</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="317" />
        
        <location filename="../src/core/jobs/job_facade_1.cpp" line="107" />
        <source>Add-on not found</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_1.cpp" line="113" />
        <source>Download the add-on first</source>
        </message>
    <message>
        
        <location filename="../src/core/library/library_facade_1.cpp" line="290" />
        <source>%1 update(s) available</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/proton_facade.cpp" line="226" />
        <source>Install Proton-GE in Settings → Launch before downloading games</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/proton_facade.cpp" line="229" />
        <source>Install %1 (Proton-GE) in Settings → Launch before downloading games</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="212" />
        <source>No download link for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/library/library_facade_2.cpp" line="74" />
        
        <location filename="../src/core/library/library_facade_2.cpp" line="98" />
        <source>Choose library folder</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="200" />
        <source>Choose game install folder</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="229" />
        <source>No game executable found in %1</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_display.cpp" line="12" />
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="245" />
        <source>Installed</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="246" />
        <source>Manual install complete for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_facade.cpp" line="309" />
        <source>Install plugin</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="223" />
        <source>Plugin install failed</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="237" />
        <source>Plugin installed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="278" />
        <source>Proton-GE installed</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="280" />
        <source>Proton-GE download failed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="407" />
        <source>Download complete — install manually</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_session_service.cpp" line="69" />
        <source>Can't install %1 — install a plugin for this source</source>
        </message>
    <message>
        
        <location filename="../src/core/install/install_session_service_install.cpp" line="39" />
        <source>Plugin not found for %1 — install it in Settings → Plugins</source>
        </message>
    <message>
        
        <location filename="../src/core/library/library_facade_2.cpp" line="52" />
        <source>Choose game executable</source>
        </message>
    <message>
        
        <location filename="../src/core/library/library_facade_2.cpp" line="55" />
        <source>Executables (*.exe *.sh *.x86_64);;All files (*)</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_controller.cpp" line="164" />
        <source>%1 · %2 games</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_controller.cpp" line="168" />
        <source>%1 sources · %2 games</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_controller.cpp" line="247" />
        <source>Catalog empty or unavailable: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_controller.cpp" line="266" />
        <source>No catalog URL configured for source %1</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="311" />
        <source>Game not found: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/runtime_dependency_service.cpp" line="111" />
        <source>Steam App ID is missing</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/runtime_dependency_service_install.cpp" line="73" />
        <source>Downloading runtime: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/runtime_dependency_service_install.cpp" line="89" />
        <source>Installer not found for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/runtime_dependency_service_install.cpp" line="96" />
        <source>Installing runtime: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/runtime_dependency_service_install.cpp" line="108" />
        <source>Proton is required to install runtime dependencies</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/runtime_dependency_service_install.cpp" line="116" />
        
        <location filename="../src/core/launch/launch_facade.cpp" line="110" />
        <source>Proton not found. Install Proton-GE in Settings → Launch.</source>
        </message>
    <message>
        
        <location filename="../src/core/launch/launch_controller.cpp" line="60" />
        <source>Game is not installed yet</source>
        </message>
    <message>
        
        <location filename="../src/core/launch/launch_controller.cpp" line="75" />
        
        <location filename="../src/core/launch/launch_facade.cpp" line="117" />
        <source>Executable not found for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/launch/launch_controller.cpp" line="82" />
        
        <location filename="../src/core/launch/launch_facade.cpp" line="132" />
        <source>Failed to launch game</source>
        </message>
    <message>
        
        <location filename="../src/core/launch/launch_controller.cpp" line="96" />
        <source>Failed to stop game</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="30" />
        <source>Unknown source: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="34" />
        <source>Source "%1" is disabled in settings</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="114" />
        <source>Could not resolve application data folder</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="137" />
        <source>Failed to delete application data</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="143" />
        <source>Failed to reset application data</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="155" />
        <source>Application data deleted. Arachnel will quit now.</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="172" />
        <source>Enter a catalog URL</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="180" />
        <source>Invalid URL — http or https required</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="202" />
        <source>Catalog entry not found: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="217" />
        <source>No Steam App ID for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="227" />
        <source>Plugin not loaded: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="235" />
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="283" />
        <source>Could not start download for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="323" />
        <source>Could not start add-on download</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="332" />
        <source>Entry not found: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_facade_2.cpp" line="347" />
        <source>Could not start update for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="197" />
        <source>No catalog sources enabled</source>
        </message>
    <message>
        
        <location filename="../src/core/library/library_facade_2.cpp" line="137" />
        <source>Found %1 game(s) on disk</source>
        </message>
    <message>
        
        <location filename="../src/core/library/library_facade_2.cpp" line="141" />
        <source>No new games found on disk</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="12" />
        <source>Download not found</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="16" />
        <source>Installation is only available for completed downloads</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="56" />
        <source>Add-on file not found</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="68" />
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="219" />
        <source>Download files not found</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="74" />
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="236" />
        <source>Could not find game to install</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/job_facade_2.cpp" line="207" />
        <source>Automatic install is unavailable. Run setup.exe from the download folder, then use the folder button to point to the game.</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_facade.cpp" line="154" />
        <source>Plugin installed</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_facade.cpp" line="157" />
        
        <location filename="../src/core/plugins/plugin_facade.cpp" line="159" />
        <source>Plugin install failed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_facade.cpp" line="173" />
        <source>Plugin removed</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_facade.cpp" line="176" />
        <source>Could not remove plugin: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_facade.cpp" line="267" />
        <source>Plugins updated</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_facade.cpp" line="311" />
        <source>Plugin files (*.arach)</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_facade.cpp" line="322" />
        <source>Could not open plugins folder</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="43" />
        <source>Invalid plugin file. Choose a plugin package (.arach)</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="73" />
        <source>Could not start archive extraction</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="85" />
        <source>Archive extraction timed out</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="96" />
        <source>Archive extraction failed (code %1)</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="139" />
        
        <location filename="../src/core/launch/process_launcher.cpp" line="20" />
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="110" />
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="218" />
        <source>File not found: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="143" />
        <source>Only .arach packages are supported</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="149" />
        <source>Failed to create temporary folder</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="161" />
        <source>Archive has no plugin.json</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="167" />
        <source>Failed to read plugin.json</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="175" />
        <source>Invalid plugin.json</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="180" />
        <source>Package is missing library %1</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="187" />
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="199" />
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="229" />
        <source>Failed to create plugin folder</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="210" />
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="235" />
        <source>Failed to copy %1</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="260" />
        <source>Failed to replace existing plugin</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="272" />
        <source>Failed to install plugin files</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="288" />
        <source>Plugin files were copied but the library failed to load. Rebuild the plugin for your Arachnel version and platform (MSVC/MinGW), then reinstall.</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="307" />
        <source>Invalid plugin id</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="335" />
        <source>Plugin is not installed</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_packages.cpp" line="327" />
        <source>Could not delete plugin files</source>
        </message>
    <message>
        
        <location filename="../src/core/torrent/torrent_session.cpp" line="139" />
        <source>No download link</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="45" />
        <source>Not checked yet</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="117" />
        <source>Checking for Arachnel updates…</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="138" />
        <source>Update check failed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="162" />
        <source>Could not parse GitHub release information</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="189" />
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="252" />
        <source>Arachnel %1 is available</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="191" />
        
        <location filename="../src/core/settings/app_updater.cpp" line="193" />
        <source>Arachnel is up to date (%1)</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="195" />
        <source>Update found, but no installer package is available for this platform</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="214" />
        <source>Open the release page to download the latest package for your platform</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="229" />
        <source>Downloading Arachnel update…</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_catalog_service.cpp" line="115" />
        <source>Could not load plugin list: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_catalog_service.cpp" line="124" />
        <source>Plugin list is invalid</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_catalog_service.cpp" line="202" />
        <source>Plugin not found in the official list</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_catalog_service.cpp" line="209" />
        <source>No download link for this plugin</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_catalog_service.cpp" line="308" />
        <source>Download failed</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_catalog_service.cpp" line="323" />
        
        <location filename="../src/core/settings/app_updater.cpp" line="294" />
        <source>Download failed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_catalog_service.cpp" line="333" />
        <source>Downloaded plugin file is empty</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_catalog_service.cpp" line="344" />
        <source>Plugin file checksum mismatch</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_catalog_service.cpp" line="260" />
        <source>Could not save plugin file</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="245" />
        
        <location filename="../src/core/settings/app_updater.cpp" line="307" />
        <source>Could not save the downloaded installer</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="296" />
        <source>Unknown error</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="316" />
        <source>Starting updater…</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="337" />
        <source>Could not start the Arachnel installer</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/app_updater.cpp" line="346" />
        <source>Automatic installer launch is only available on Windows</source>
        </message>
    <message>
        
        <location filename="../src/core/install/online_fix_overlay.cpp" line="224" />
        <source>Not installed</source>
        </message>
    <message>
        
        <location filename="../src/core/install/online_fix_overlay.cpp" line="226" />
        <source>Enabled</source>
        </message>
    <message>
        
        <location filename="../src/core/install/online_fix_overlay.cpp" line="228" />
        <source>Disabled</source>
        </message>
    <message>
        
        <location filename="../src/core/install/online_fix_overlay.cpp" line="208" />
        <source>Online Fix overlay not found in this install</source>
        </message>
    <message>
        
        <location filename="../src/core/install/online_fix_overlay.cpp" line="129" />
        <source>Failed to enable Online Fix: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/install/online_fix_overlay.cpp" line="141" />
        <source>Failed to disable Online Fix: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/settings/settings_store_persistence.cpp" line="108" />
        <source>FreeTP torrent catalog — magnet links and add-ons</source>
        </message>
    <message>
        
        <location filename="../src/core/library/storage_library.cpp" line="34" />
        <source>Library</source>
        </message>
    <message>
        
        <location filename="../src/core/library/storage_library.cpp" line="45" />
        <source>Disk</source>
        </message>
    <message>
        
        <location filename="../src/core/util/file_utils.cpp" line="24" />
        <source>Failed to delete file: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/util/file_utils.cpp" line="31" />
        <source>Failed to delete folder: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/util/file_utils.cpp" line="42" />
        <source>Source not found: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/util/file_utils.cpp" line="50" />
        <source>Failed to replace: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/util/file_utils.cpp" line="56" />
        <source>Failed to copy: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/util/file_utils.cpp" line="64" />
        <source>Failed to create folder: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_feed_loader.cpp" line="88" />
        <source>Catalog is empty or format not recognized</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_parser.cpp" line="291" />
        <source>Empty server response</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_parser.cpp" line="302" />
        <source>Invalid JSON</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_parser.cpp" line="306" />
        <source>No downloads array — not a Hydra catalog</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_parser.cpp" line="310" />
        <source>downloads array is empty</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog/catalog_parser.cpp" line="314" />
        <source>Failed to parse games from catalog</source>
        </message>
    <message>
        
        <location filename="../src/core/jobs/http_download_session.cpp" line="126" />
        <source>Failed to save file</source>
        </message>
    <message>
        
        <location filename="../src/core/plugins/plugin_host_async.cpp" line="38" />
        
        <location filename="../src/core/plugins/plugin_host_async.cpp" line="60" />
        
        <location filename="../src/core/plugins/plugin_host_async.cpp" line="82" />
        <source>Plugin not found</source>
        </message>
    <message>
        
        <location filename="../src/core/launch/process_launcher.cpp" line="13" />
        <source>Executable is not set</source>
        </message>
    <message>
        
        <location filename="../src/core/launch/process_launcher.cpp" line="37" />
        <source>Failed to start process</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="26" />
        <source>Failed to start: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="32" />
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="152" />
        <source>Timeout: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="40" />
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="162" />
        <source>%1 exited with code %2</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="99" />
        <source>launch cancelled (UAC)</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="101" />
        <source>administrator rights required</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="134" />
        <source>Failed to start %1: %2</source>
        </message>
    <message>
        
        <location filename="../src/core/runtime/windows_runner.cpp" line="142" />
        <source>Could not track installer process</source>
        </message>
    <message>
        
        <location filename="../src/core/library/library_controller.cpp" line="222" />
        
        <location filename="../src/core/library/library_controller.cpp" line="243" />
        
        <location filename="../src/core/facade/core_wiring_services.cpp" line="155" />
        <source>Game removed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/library/library_controller.cpp" line="230" />
        <source>Removing “%1”…</source>
        </message>
    <message>
        
        <location filename="../src/core/library/library_controller.cpp" line="368" />
        <source>Drive removed. %1 game(s) kept on disk and listed under another drive.</source>
        </message>
    <message>
        
        <location filename="../src/core/library/library_controller.cpp" line="373" />
        <source>Drive removed</source>
        </message>

</context>
<context>
    <name>CrashReportDialog</name>
    <message>
        
        <location filename="../qml/components/CrashReportDialog.qml" line="14" />
        <source>Application crashed</source>
        </message>

</context>
<context>
    <name>CrashReportPanel</name>
    <message>
        
        <location filename="../qml/components/CrashReportPanel.qml" line="28" />
        <source>Application crashed</source>
        </message>
    <message>
        
        <location filename="../qml/components/CrashReportPanel.qml" line="35" />
        
        <location filename="../qml/components/CrashReportPanel.qml" line="47" />
        <source>Arachnel has crashed.</source>
        </message>
    <message>
        
        <location filename="../qml/components/CrashReportPanel.qml" line="36" />
        
        <location filename="../qml/components/CrashReportPanel.qml" line="48" />
        <source>Arachnel stopped unexpectedly during the last session.</source>
        </message>
    <message>
        
        <location filename="../qml/components/CrashReportPanel.qml" line="93" />
        <source>Report file: %1</source>
        </message>
    <message>
        
        <location filename="../qml/components/CrashReportPanel.qml" line="106" />
        <source>Dismiss</source>
        </message>
    <message>
        
        <location filename="../qml/components/CrashReportPanel.qml" line="112" />
        <source>Open folder</source>
        </message>
    <message>
        
        <location filename="../qml/components/CrashReportPanel.qml" line="118" />
        <source>Copy report</source>
        </message>
    <message>
        
        <location filename="../qml/components/CrashReportPanel.qml" line="126" />
        <source>Create GitHub issue</source>
        </message>

</context>
<context>
    <name>CrashReportWindow</name>
    <message>
        
        <location filename="../qml/app/CrashReportWindow.qml" line="16" />
        <source>Application crashed</source>
        </message>

</context>
<context>
    <name>DownloadJobCard</name>
    <message>
        
        <location filename="../qml/components/DownloadJobCard.qml" line="81" />
        <source>Unknown download</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadJobCard.qml" line="98" />
        <source>Install failed</source>
        </message>

</context>
<context>
    <name>DownloadJobGroupCard</name>
    <message>
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="48" />
        <source>%1 add-ons · %2 downloading</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="50" />
        <source>%1 add-ons · done</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="51" />
        <source>%1 add-ons</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="196" />
        <source>Add-ons</source>
        </message>

</context>
<context>
    <name>DownloadProgressButton</name>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="19" />
        <source>Download</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="151" />
        <source>Installing…</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="153" />
        <source>Retry install</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="155" />
        <source>Install</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="157" />
        <source>Downloaded</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="159" />
        <source>Paused · %1%</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="161" />
        <source>Downloading · %1%</source>
        </message>

</context>
<context>
    <name>DownloadsPage</name>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="125" />
        <source>No downloads</source>
        </message>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="158" />
        <source>Downloads</source>
        </message>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="167" />
        <source>%1 active · %2 finished · resume after restart</source>
        </message>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="170" />
        <source>%1 active · resume after restart</source>
        </message>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="172" />
        <source>%1 finished · torrents resume after restart</source>
        </message>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="173" />
        <source>Torrents resume after restart</source>
        </message>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="183" />
        <source>Clear finished</source>
        </message>

</context>
<context>
    <name>GameDetailsContent</name>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="39" />
        <source>Game details</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="66" />
        <source>Game not found</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="99" />
        <source>%1 add-ons</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="108" />
        <source>Steam CDN · Online Fix</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="119" />
        <source>Update available</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="135" />
        <source>Source page</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="136" />
        <source>Source website</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="147" />
        <source>Steam</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="177" />
        <source>Install failed</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="191" />
        <source>Stop</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="195" />
        <source>PreparingтАж</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="196" />
        <source>Play</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="238" />
        
        <location filename="../qml/app/GameDetailsContent.qml" line="358" />
        <source>Delete</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="255" />
        <source>Update</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="307" />
        <source>Description</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="313" />
        <source>Description is not available yet.</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="330" />
        <source>Remove game?</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="352" />
        <source>Cancel</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsContent.qml" line="168" />
        <source>Ready to download from Steam CDN. Online Fix can be included when needed.</source>
        </message>

</context>
<context>
    <name>GameDetailsMediaPreview</name>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaPreview.qml" line="75" />
        <source>Gameplay video</source>
        </message>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaPreview.qml" line="189" />
        
        <location filename="../qml/components/GameDetailsMediaPreview.qml" line="362" />
        <source>Close</source>
        </message>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaPreview.qml" line="207" />
        
        <location filename="../qml/components/GameDetailsMediaPreview.qml" line="371" />
        <source>Open in browser</source>
        </message>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaPreview.qml" line="253" />
        <source>Screenshot %1 of %2</source>
        </message>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaPreview.qml" line="256" />
        <source>Screenshots</source>
        </message>

</context>
<context>
    <name>GameDetailsMediaSection</name>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaSection.qml" line="77" />
        <source>Screenshots</source>
        </message>

</context>
<context>
    <name>GameSettingsRuntimePanel</name>
    <message>
        
        <location filename="../qml/app/GameSettingsRuntimePanel.qml" line="35" />
        <source>Runtime container</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsRuntimePanel.qml" line="41" />
        <source>Proton prefix and redistributables for this game (Linux only).</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsRuntimePanel.qml" line="53" />
        <source>Container</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsRuntimePanel.qml" line="79" />
        <source>Prefix</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsRuntimePanel.qml" line="91" />
        <source>%1 (not created yet)</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsRuntimePanel.qml" line="105" />
        <source>Steam App ID</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsRuntimePanel.qml" line="124" />
        <source>No runtime dependencies detected for this game.</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsRuntimePanel.qml" line="125" />
        <source>Dependencies: %1 / %2 installed</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsRuntimePanel.qml" line="146" />
        <source>Installed</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsRuntimePanel.qml" line="146" />
        <source>Missing</source>
        </message>

</context>
<context>
    <name>GameSettingsSheet</name>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="87" />
        <source>Game settings</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="119" />
        <source>Auto-update this game</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="125" />
        <source>When enabled, updates start automatically after the catalog loads.</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="191" />
        <source>Proton</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="197" />
        <source>Override Proton for this game. Default uses Settings → Launch.</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="208" />
        <source>Default</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="247" />
        <source>Launch options</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="254" />
        <source>Extra launch arguments for this game</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="265" />
        <source>Custom executable (optional)</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="310" />
        <source>Information</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="316" />
        <source>Source</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="317" />
        <source>Version</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="318" />
        <source>Size</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="319" />
        <source>Install type</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="327" />
        <source>Install path</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="331" />
        <source>Installing…</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="333" />
        <source>Waiting to install</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="334" />
        <source>—</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="337" />
        <source>Download</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="371" />
        <source>Done</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="149" />
        <source>Online Fix for this game</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="155" />
        <source>When disabled, SteamFix/winmm overlay DLLs are renamed so the game runs without the fix.</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="321" />
        <source>Online Fix</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="323" />
        <source>Not installed</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="324" />
        <source>Not needed</source>
        </message>

</context>
<context>
    <name>InstallAddonSelectionSheet</name>
    <message>
        
        <location filename="../qml/settings/InstallAddonSelectionSheet.qml" line="63" />
        <source>Add-ons</source>
        </message>
    <message>
        
        <location filename="../qml/settings/InstallAddonSelectionSheet.qml" line="73" />
        <source>Choose add-ons to download together with the game.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/InstallAddonSelectionSheet.qml" line="87" />
        <source>All</source>
        </message>
    <message>
        
        <location filename="../qml/settings/InstallAddonSelectionSheet.qml" line="93" />
        <source>Deselect</source>
        </message>
    <message>
        
        <location filename="../qml/settings/InstallAddonSelectionSheet.qml" line="174" />
        <source>Optional</source>
        </message>
    <message>
        
        <location filename="../qml/settings/InstallAddonSelectionSheet.qml" line="211" />
        <source>Cancel</source>
        </message>
    <message>
        
        <location filename="../qml/settings/InstallAddonSelectionSheet.qml" line="218" />
        <source>Next</source>
        </message>

</context>
<context>
    <name>InstallLocationSheet</name>
    <message>
        
        <location filename="../qml/settings/InstallLocationSheet.qml" line="37" />
        
        <location filename="../qml/settings/InstallLocationSheet.qml" line="151" />
        <source>Install</source>
        </message>
    <message>
        
        <location filename="../qml/settings/InstallLocationSheet.qml" line="45" />
        <source>Choose a drive for installation</source>
        </message>
    <message>
        
        <location filename="../qml/settings/InstallLocationSheet.qml" line="55" />
        <source>Install to:</source>
        </message>
    <message>
        
        <location filename="../qml/settings/InstallLocationSheet.qml" line="144" />
        <source>Cancel</source>
        </message>

</context>
<context>
    <name>LibraryContent</name>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="114" />
        <source>Running</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="153" />
        <source>Play</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="164" />
        <source>Details</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="172" />
        <source>Update</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="210" />
        <source>In library</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="217" />
        <source>Sources</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="224" />
        <source>Tasks</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="231" />
        <source>Updates</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="263" />
        <source>%1 active downloads</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="270" />
        <source>Downloads continue after restart</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="277" />
        <source>Open</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="294" />
        <source>My library</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryContent.qml" line="299" />
        <source>%1 games</source>
        </message>

</context>
<context>
    <name>LibraryEmptyState</name>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="75" />
        <source>Nothing here yet</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="95" />
        <source>Open catalog</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="96" />
        <source>Install plugin</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="111" />
        <source>Settings</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="119" />
        <source>Catalogs and plugins</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="135" />
        <source>Step 1</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="136" />
        <source>Plugin</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="141" />
        <source>Step 2</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="142" />
        <source>Catalog</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="147" />
        <source>Step 3</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryEmptyState.qml" line="148" />
        <source>Library</source>
        </message>

</context>
<context>
    <name>LibraryGameCard</name>
    <message>
        
        <location filename="../qml/components/LibraryGameCard.qml" line="25" />
        
        <location filename="../qml/components/LibraryGameCard.qml" line="28" />
        <source>%1 add-ons</source>
        </message>
    <message>
        
        <location filename="../qml/components/LibraryGameCard.qml" line="27" />
        <source>%1/%2 add-ons</source>
        </message>
    <message>
        
        <location filename="../qml/components/LibraryGameCard.qml" line="60" />
        <source>Installing %1%</source>
        </message>
    <message>
        
        <location filename="../qml/components/LibraryGameCard.qml" line="63" />
        <source>Installing…</source>
        </message>
    <message>
        
        <location filename="../qml/components/LibraryGameCard.qml" line="65" />
        <source>Paused · %1%</source>
        </message>
    <message>
        
        <location filename="../qml/components/LibraryGameCard.qml" line="66" />
        <source>Downloading %1%</source>
        </message>
    <message>
        
        <location filename="../qml/components/LibraryGameCard.qml" line="124" />
        <source>Playing</source>
        </message>
    <message>
        
        <location filename="../qml/components/LibraryGameCard.qml" line="143" />
        <source>Updating</source>
        </message>
    <message>
        
        <location filename="../qml/components/LibraryGameCard.qml" line="195" />
        <source>Running</source>
        </message>

</context>
<context>
    <name>LibraryPage</name>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="31" />
        <source>Playing now</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="32" />
        <source>Recently played</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="36" />
        <source>Nothing played yet</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="43" />
        <source>Launch a game from your library — it will appear here.</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="75" />
        <source>Installing %1%</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="78" />
        <source>Installing…</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="80" />
        <source>Paused · %1%</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="81" />
        <source>Downloading %1%</source>
        </message>

</context>
<context>
    <name>NotificationsPopup</name>
    <message>
        
        <location filename="../qml/components/NotificationsPopup.qml" line="46" />
        <source>Notifications</source>
        </message>
    <message>
        
        <location filename="../qml/components/NotificationsPopup.qml" line="88" />
        <source>Empty for now</source>
        </message>
    <message>
        
        <location filename="../qml/components/NotificationsPopup.qml" line="97" />
        <source>Installs, errors, and other events will appear here.</source>
        </message>

</context>
<context>
    <name>OnboardingBasicsStep</name>
    <message>
        
        <location filename="../qml/onboarding/OnboardingBasicsStep.qml" line="28" />
        <source>A quick setup before you start</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingBasicsStep.qml" line="34" />
        <source>A quick setup: language, storage, plugins, and a few defaults. Change anything later in Settings.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingBasicsStep.qml" line="48" />
        <source>Language</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingBasicsStep.qml" line="53" />
        <source>Choose the interface language.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingBasicsStep.qml" line="81" />
        <source>Appearance</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingBasicsStep.qml" line="86" />
        <source>Pick light or dark theme, palette, and accent color. Change later in Settings.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingBasicsStep.qml" line="96" />
        <source>Dark</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingBasicsStep.qml" line="102" />
        <source>Light</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingBasicsStep.qml" line="110" />
        <source>Palette</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingBasicsStep.qml" line="141" />
        <source>Primary</source>
        </message>

</context>
<context>
    <name>OnboardingFinalSteps</name>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="23" />
        <source>Updates</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="26" />
        <source>Recommended defaults — change anytime in Settings → Updates.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="34" />
        <source>Check for game updates</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="35" />
        <source>Notify you when a newer build is available.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="40" />
        <source>Check for Arachnel updates</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="41" />
        <source>Check for new Arachnel versions automatically.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="76" />
        <source>Proton (Linux)</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="79" />
        <source>Windows games need Proton on Linux. Install it now or later in Settings → Launch.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="87" />
        <source>Proton ready: %1</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="96" />
        <source>Downloading Proton… %1%</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="104" />
        <source>Proton already installed</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="105" />
        <source>Download Proton-GE %1</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="106" />
        <source>Download Proton-GE</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="115" />
        <source>I'll do this later</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="125" />
        <source>You're all set</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="128" />
        <source>Open Catalog to browse games. Change language, storage, and plugins anytime in Settings.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingFinalSteps.qml" line="135" />
        <source>Tip: with a plugin installed, Install runs automatically after download. Catalog-only setups need a manual Install step.</source>
        </message>

</context>
<context>
    <name>OnboardingSheet</name>
    <message>
        
        <location filename="../qml/onboarding/OnboardingSheet.qml" line="142" />
        <source>Welcome to Arachnel</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingSheet.qml" line="148" />
        <source>Step %1 of %2</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingSheet.qml" line="155" />
        <source>Skip</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingSheet.qml" line="227" />
        <source>Back</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingSheet.qml" line="236" />
        <source>Get started</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingSheet.qml" line="236" />
        <source>Next</source>
        </message>

</context>
<context>
    <name>OnboardingStoragePluginsStep</name>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="27" />
        <source>Game library folder</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="32" />
        <source>Choose where games are installed. Downloads go to a subfolder on the same drive.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="90" />
        <source>Choose folder…</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="103" />
        <source>Or keep the default path already listed above.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="116" />
        <source>Source plugins</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="121" />
        <source>Plugins enable automatic install and Play (e.g. FreeTP). Without one, you can still browse catalogs and install manually.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="144" />
        <source>Official plugins</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="149" />
        <source>Refresh list</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="157" />
        <source>Loading official plugins…</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="174" />
        <source>No official plugins available for this platform.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="207" />
        <source>v%1</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="214" />
        <source>Installed</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="214" />
        <source>Installing…</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="214" />
        <source>Install</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="222" />
        <source>Or install a plugin file you already have.</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="249" />
        <source>v%1 · %2</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="264" />
        <source>Install plugin…</source>
        </message>
    <message>
        
        <location filename="../qml/onboarding/OnboardingStoragePluginsStep.qml" line="270" />
        <source>Skip for now</source>
        </message>

</context>
<context>
    <name>PluginInstallOverlay</name>
    <message>
        
        <location filename="../qml/app/PluginInstallOverlay.qml" line="49" />
        <source>Installing plugin “%1”…</source>
        </message>
    <message>
        
        <location filename="../qml/app/PluginInstallOverlay.qml" line="50" />
        
        <location filename="../qml/app/PluginInstallOverlay.qml" line="52" />
        <source>Installing plugin…</source>
        </message>
    <message>
        
        <location filename="../qml/app/PluginInstallOverlay.qml" line="60" />
        <source>Downloading and unpacking. The UI stays responsive — please wait.</source>
        </message>

</context>
<context>
    <name>ProtonRequiredDialog</name>
    <message>
        
        <location filename="../qml/components/ProtonRequiredDialog.qml" line="13" />
        <source>Proton required</source>
        </message>
    <message>
        
        <location filename="../qml/components/ProtonRequiredDialog.qml" line="19" />
        <source>latest Proton-GE</source>
        </message>
    <message>
        
        <location filename="../qml/components/ProtonRequiredDialog.qml" line="33" />
        <source>Games run through Proton on Linux. Install %1 before downloading.</source>
        </message>
    <message>
        
        <location filename="../qml/components/ProtonRequiredDialog.qml" line="35" />
        <source>Games run through Proton on Linux. Install Proton-GE before downloading.</source>
        </message>
    <message>
        
        <location filename="../qml/components/ProtonRequiredDialog.qml" line="43" />
        <source>Currently installed: %1</source>
        </message>
    <message>
        
        <location filename="../qml/components/ProtonRequiredDialog.qml" line="61" />
        <source>Cancel</source>
        </message>
    <message>
        
        <location filename="../qml/components/ProtonRequiredDialog.qml" line="68" />
        <source>Settings</source>
        </message>
    <message>
        
        <location filename="../qml/components/ProtonRequiredDialog.qml" line="78" />
        <source>Downloading…</source>
        </message>
    <message>
        
        <location filename="../qml/components/ProtonRequiredDialog.qml" line="79" />
        <source>Download %1</source>
        </message>

</context>
<context>
    <name>RunningGameBar</name>
    <message>
        
        <location filename="../qml/components/RunningGameBar.qml" line="65" />
        <source>Playing now</source>
        </message>
    <message>
        
        <location filename="../qml/components/RunningGameBar.qml" line="83" />
        <source>Stop</source>
        </message>

</context>
<context>
    <name>SettingsAboutPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="16" />
        <source>Windows</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="18" />
        <source>Linux</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="20" />
        <source>macOS</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="42" />
        <source>Browse catalogs, download games, and launch from your library.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="72" />
        <source>Application</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="92" />
        <source>Version</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="98" />
        <source>Unknown</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="112" />
        <source>Platform</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="131" />
        <source>Danger zone</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="156" />
        <source>Delete application data</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="162" />
        <source>Deletes settings, download history, caches, plugins, and Proton from the app folder. Game files on your disks stay. Arachnel will quit afterward.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="179" />
        <source>Delete application data…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="190" />
        <source>Delete application data?</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="198" />
        <source>This cannot be undone. Settings, plugins, caches, and library records will be removed. Game files on disk stay in place.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="215" />
        <source>Cancel</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAboutPage.qml" line="222" />
        <source>Delete and quit</source>
        </message>

</context>
<context>
    <name>SettingsAppearancePage</name>
    <message>
        
        <location filename="../qml/settings/SettingsAppearancePage.qml" line="15" />
        <source>English</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAppearancePage.qml" line="16" />
        <source>Russian</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAppearancePage.qml" line="46" />
        <source>Theme and colors apply across the app.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAppearancePage.qml" line="59" />
        <source>Dark theme</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAppearancePage.qml" line="59" />
        <source>Light theme</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAppearancePage.qml" line="81" />
        <source>Palette</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAppearancePage.qml" line="124" />
        <source>Primary</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAppearancePage.qml" line="159" />
        <source>Language</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAppearancePage.qml" line="194" />
        <source>Community translations</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsAppearancePage.qml" line="210" />
        <source>Help translate</source>
        </message>

</context>
<context>
    <name>SettingsHubPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="18" />
        <source>Plugins</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="23" />
        <source>Hydra catalogs</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="19" />
        <source>FreeTP and others — install, launch, and add-ons</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="24" />
        <source>Catalog links — import from Hydra or elsewhere</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="28" />
        <source>Storage</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="29" />
        <source>Library and download folders</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="33" />
        <source>Updates</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="34" />
        <source>Game and launcher updates</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="39" />
        <source>Launch options and Proton on Linux</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="44" />
        <source>Theme, colors, and language</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="49" />
        <source>Version and app data</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="38" />
        <source>Launch</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="43" />
        <source>Appearance</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="48" />
        <source>About</source>
        </message>

</context>
<context>
    <name>SettingsLaunchPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="59" />
        <source>Linux: all games run through Proton (Windows builds).</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="91" />
        <source>Default: %1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="101" />
        <source>Required before download: %1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="102" />
        <source>Install Proton-GE before downloading games.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="110" />
        <source>Pick default Proton and drag priority with arrows. Steam installs are detected automatically.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="122" />
        <source>Download %1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="130" />
        <source>Rescan</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="228" />
        <source>No Proton found. Download Proton-GE or install Proton in Steam.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="84" />
        <source>Proton runtime</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="38" />
        <source>Extra options added to every game launch.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="49" />
        <source>Launch options</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="123" />
        <source>Download Proton-GE</source>
        </message>

</context>
<context>
    <name>SettingsPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="25" />
        
        <location filename="../qml/settings/SettingsPage.qml" line="28" />
        
        <location filename="../qml/settings/SettingsPage.qml" line="131" />
        <source>Settings</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="140" />
        <source>Plugins</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="149" />
        <source>Plugin store</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="157" />
        <source>Hydra catalogs</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="169" />
        <source>Edit catalog</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="169" />
        <source>New Hydra catalog</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="179" />
        <source>Storage</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="187" />
        <source>Updates</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="195" />
        <source>Launch</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="203" />
        <source>Appearance</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="211" />
        <source>About</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="265" />
        <source>Back</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="278" />
        <source>Done</source>
        </message>

</context>
<context>
    <name>SettingsPluginStorePage</name>
    <message>
        
        <location filename="../qml/settings/SettingsPluginStorePage.qml" line="54" />
        <source>Official plugins from the Arachnel catalog. Install adds them to your plugins folder.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginStorePage.qml" line="68" />
        <source>Available</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginStorePage.qml" line="74" />
        <source>Refresh list</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginStorePage.qml" line="85" />
        <source>Loading official plugins…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginStorePage.qml" line="108" />
        <source>No official plugins available for this platform.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginStorePage.qml" line="174" />
        <source>v%1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginStorePage.qml" line="182" />
        <source>Installed</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginStorePage.qml" line="183" />
        <source>Installing…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginStorePage.qml" line="183" />
        <source>Install</source>
        </message>

</context>
<context>
    <name>SettingsPluginsPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="167" />
        <source>v%1 · %2</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="56" />
        <source>Plugin store</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="82" />
        <source>No plugins installed</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="89" />
        <source>Open the plugin store or install a plugin file you already have.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="105" />
        <source>Installed plugins</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="166" />
        <source>v%1 · %2 — not loaded</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="185" />
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="294" />
        <source>Delete</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="219" />
        <source>Install from file…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="230" />
        <source>Open folder</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="237" />
        <source>Refresh</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="247" />
        <source>User-installed: %1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="261" />
        <source>Remove plugin?</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="269" />
        <source>Remove "%1"? Catalogs from this plugin will stop working until you install it again.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="287" />
        <source>Cancel</source>
        </message>

</context>
<context>
    <name>SettingsSourceFormPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="61" />
        <source>Enter a name and catalog URL.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="71" />
        <source>Validating catalog…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="86" />
        <source>Could not save changes.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="92" />
        <source>Could not add catalog.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="113" />
        <source>Could not load catalog from this URL.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="117" />
        <source>Games found: %1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="143" />
        <source>Name</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="152" />
        <source>Catalog URL</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="161" />
        <source>Short description (optional)</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="186" />
        <source>Cancel</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="193" />
        <source>Validating…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="194" />
        <source>Save</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourceFormPage.qml" line="194" />
        <source>Add</source>
        </message>

</context>
<context>
    <name>SettingsSourcesPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsSourcesPage.qml" line="28" />
        <source>Games: …</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourcesPage.qml" line="29" />
        <source>Games: %1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourcesPage.qml" line="85" />
        <source>No catalogs yet</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourcesPage.qml" line="171" />
        <source>Plugin · v%1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourcesPage.qml" line="201" />
        <source>No URL — catalog will not load</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourcesPage.qml" line="203" />
        <source>Active in catalog</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourcesPage.qml" line="204" />
        <source>Disabled</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourcesPage.qml" line="214" />
        <source>Edit</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourcesPage.qml" line="226" />
        <source>Delete</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsSourcesPage.qml" line="241" />
        <source>Add Hydra catalog</source>
        </message>

</context>
<context>
    <name>SettingsStoragePage</name>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="191" />
        <source>Add drive…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="394" />
        <source>Remove drive?</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="403" />
        <source>“%1” still has games (%2). Remove the drive from Arachnel anyway? Files stay on disk; games stay in the library under another drive.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="406" />
        <source>Remove “%1” from Arachnel? Files on disk are not deleted.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="432" />
        <source>Remove</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="203" />
        <source>Scan for installed games</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="220" />
        <source>Games: %1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="307" />
        <source>No games on this drive yet</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="325" />
        <source>Delete</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="339" />
        <source>Move…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="358" />
        <source>Move to drive</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="425" />
        <source>Cancel</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="432" />
        <source>Remove anyway</source>
        </message>

</context>
<context>
    <name>SettingsUpdatesPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="38" />
        <source>Games</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="54" />
        <source>Check for updates when loading the catalog</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="85" />
        <source>Install updates automatically on launch</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="28" />
        <source>Check for game updates and new Arachnel versions.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="60" />
        <source>Shows when a newer build is available in the catalog.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="91" />
        <source>Downloads updates when you open the catalog. You can turn this off per game.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="111" />
        <source>Check for game updates</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="129" />
        <source>Arachnel</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="137" />
        <source>Current version: %1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="200" />
        <source>Check for Arachnel updates on startup</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="206" />
        <source>Checks for new versions in the background.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="232" />
        <source>Check for Arachnel updates</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="241" />
        <source>Download and install</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="252" />
        <source>Open release page</source>
        </message>

</context>
<context>
    <name>Setup</name>
    <message>
        
        <location filename="../setup/src/self_extractor.cpp" line="140" />
        <source>Extracting files…</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="411" />
        <source>Updating uninstaller…</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="412" />
        <source>Registering uninstaller…</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="424" />
        <source>Refreshing shortcuts…</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="425" />
        <source>Creating shortcuts…</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="430" />
        <source>Update complete</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="431" />
        <source>Installation complete</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="449" />
        
        <location filename="../setup/src/setup_backend.cpp" line="492" />
        <source>Please wait — updating Arachnel…</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="450" />
        
        <location filename="../setup/src/setup_backend.cpp" line="493" />
        <source>Preparing…</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="484" />
        <source>Waiting for Arachnel to close…</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="486" />
        <source>Arachnel is still running. Close it and try again.</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="499" />
        <source>Clearing install folder…</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="501" />
        <source>Could not clear existing install folder</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="507" />
        <source>Creating install folder…</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="510" />
        <source>Could not create install folder</source>
        </message>
    <message>
        
        <location filename="../setup/src/setup_backend.cpp" line="524" />
        <source>Finalizing…</source>
        </message>

</context>
<context>
    <name>SetupTitleBar</name>
    <message>
        
        <location filename="../setup/qml/components/SetupTitleBar.qml" line="42" />
        <source>Arachnel Setup</source>
        </message>

</context>
<context>
    <name>SetupWindow</name>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="15" />
        <source>Arachnel Setup</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="66" />
        <source>Choose language</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="72" />
        <source>Select the installer language.</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="99" />
        <source>Install Arachnel</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="105" />
        <source>Game launcher with plugin-based sources. This wizard unpacks Arachnel to your computer.</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="114" />
        <source>No embedded app payload found. Build the installer with run.ps1 --installer.</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="128" />
        <source>Choose install location</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="140" />
        <source>Install folder</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="156" />
        <source>Shortcuts</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="161" />
        <source>Create desktop shortcut</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="167" />
        <source>Create Start Menu shortcut</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="180" />
        <source>Updating Arachnel…</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="180" />
        <source>Installing…</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="187" />
        <source>Please wait while Arachnel is updated. Do not close this window.</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="188" />
        <source>Arachnel is being installed on your computer.</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="246" />
        <source>Arachnel is up to date</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="246" />
        <source>Arachnel is ready</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="269" />
        <source>Back</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="276" />
        
        <location filename="../setup/qml/SetupWindow.qml" line="283" />
        <source>Continue</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="291" />
        <source>Install</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="299" />
        <source>Open folder</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="306" />
        <source>Launch</source>
        </message>
    <message>
        
        <location filename="../setup/qml/SetupWindow.qml" line="316" />
        <source>Finish</source>
        </message>

</context>
<context>
    <name>SourceHelpDialog</name>
    <message>
        
        <location filename="../qml/components/SourceHelpDialog.qml" line="12" />
        <source>Catalogs and plugins</source>
        </message>
    <message>
        
        <location filename="../qml/components/SourceHelpDialog.qml" line="19" />
        <source>Step 1</source>
        </message>
    <message>
        
        <location filename="../qml/components/SourceHelpDialog.qml" line="20" />
        <source>Hydra catalog</source>
        </message>
    <message>
        
        <location filename="../qml/components/SourceHelpDialog.qml" line="25" />
        <source>Step 2</source>
        </message>
    <message>
        
        <location filename="../qml/components/SourceHelpDialog.qml" line="26" />
        <source>Catalog</source>
        </message>
    <message>
        
        <location filename="../qml/components/SourceHelpDialog.qml" line="31" />
        <source>Step 3</source>
        </message>
    <message>
        
        <location filename="../qml/components/SourceHelpDialog.qml" line="32" />
        <source>Library</source>
        </message>

</context>
<context>
    <name>SteamidraTrustSheet</name>
    <message>
        
        <location filename="../qml/settings/SteamidraTrustSheet.qml" line="25" />
        <source>Steam CDN + Online Fix</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SteamidraTrustSheet.qml" line="59" />
        <source>Got it</source>
        </message>

</context>
</TS>
