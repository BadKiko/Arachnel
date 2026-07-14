<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US">
<context>
    <name />
    <message id="help.catalog_intro">
        
        <location filename="../qml/i18n/Messages.qml" line="8" />
        <source>Two ways to fill the catalog: Hydra catalogs (games.json) for migrating from Hydra; plugins (.arach) for the full cycle: catalog, install, launch, and add-ons.</source>
        </message>
    <message id="help.hydra_catalog_body">
        
        <location filename="../qml/i18n/Messages.qml" line="11" />
        <source>A games.json feed by URL — the same format as Hydra Launcher. Add it under Settings → Hydra catalogs; games appear in Catalog.</source>
        </message>
    <message id="help.catalog_body">
        
        <location filename="../qml/i18n/Messages.qml" line="14" />
        <source>Games from enabled catalogs appear in Catalog. Downloads use torrent magnets from JSON.</source>
        </message>
    <message id="help.library_body">
        
        <location filename="../qml/i18n/Messages.qml" line="17" />
        <source>After download and install, the game appears here — launch, updates, and details.</source>
        </message>
    <message id="settings.sources.connect_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="20" />
        <source>Connect catalogs in Hydra Launcher format (games.json). Handy when migrating from Hydra — same game links, torrent downloads. A source plugin is required to install and launch.</source>
        </message>
    <message id="settings.sources.add_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="23" />
        <source>Click Add catalog and paste your games.json URL — like Hydra, or a public community feed.</source>
        </message>
    <message id="settings.source_form.desc">
        
        <location filename="../qml/i18n/Messages.qml" line="26" />
        <source>Hydra catalog — a games.json JSON feed by URL. Arachnel pulls the game list and magnet links like Hydra Launcher. Install and launch via a source plugin (e.g. FreeTP).</source>
        </message>
    <message id="settings.appearance.weblate_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="29" />
        <source>Missing your language? Help translate Arachnel on &lt;a href="%1"&gt;Weblate&lt;/a&gt; or send a pull request with translations/*.ts files.</source>
        </message>
    <message id="settings.plugins.install_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="32" />
        <source>Install the .arach package using the button below.

Build the plugin from the arachnel-plugin-freetp repository (dist/freetp.arach).</source>
        </message>
    <message id="settings.plugins.desc">
        
        <location filename="../qml/i18n/Messages.qml" line="35" />
        <source>Plugins are sources with catalog, install, and launch. Package: .arach file (ZIP with plugin.json and libraries).</source>
        </message>
    <message id="library.empty.subtitle">
        
        <location filename="../qml/i18n/Messages.qml" line="38" />
        <source>Your library is empty. Install a source plugin, pick a game in the catalog, and it will show up here.</source>
        </message>
    <message id="library.step1.body">
        
        <location filename="../qml/i18n/Messages.qml" line="41" />
        <source>Install a source plugin (FreeTP, etc.) under Settings → Plugins.</source>
        </message>
    <message id="library.step2.body">
        
        <location filename="../qml/i18n/Messages.qml" line="44" />
        <source>Pick a game and start installation — the torrent downloads automatically.</source>
        </message>
    <message id="library.step3.body">
        
        <location filename="../qml/i18n/Messages.qml" line="47" />
        <source>Installed games live here: launch, updates, and details.</source>
        </message>
    <message id="game.install.torrent_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="50" />
        <source>Torrent downloaded. Click Install for the source plugin to extract or install the game.</source>
        </message>
    <message id="catalog.pipeline_desc">
        
        <location filename="../qml/i18n/Messages.qml" line="53" />
        <source>The source defines how installation works — each plugin has its own pipeline.</source>
        </message>
    <message id="catalog.connect_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="56" />
        <source>Connect a Hydra catalog in Settings — or install a source plugin.</source>
        </message>
    <message id="catalog.enable_chips_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="59" />
        <source>Enable one or more source chips — or leave them all off.</source>
        </message>
    <message id="storage.libraries_desc">
        
        <location filename="../qml/i18n/Messages.qml" line="62" />
        <source>Libraries on disks — like Steam. Default is C:; you can add other drives.</source>
        </message>
    <message id="addons.selection_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="65" />
        <source>Add-ons are available for "%1" — choose what to download with the game.</source>
        </message>
    <message id="downloads.empty_hint">
        
        <location filename="../qml/i18n/Messages.qml" line="68" />
        <source>Start installing from the catalog — progress will appear here.</source>
        </message>
    <message id="game.delete_warning">
        
        <location filename="../qml/i18n/Messages.qml" line="71" />
        <source>Game files will be deleted from disk. This cannot be undone.</source>
        </message>

</context>
<context>
    <name>AppWindow</name>
    <message>
        
        <location filename="../qml/app/AppWindow.qml" line="16" />
        <source>Arachnel</source>
        </message>
    <message>
        
        <location filename="../qml/app/AppWindow.qml" line="96" />
        <source>Library</source>
        </message>
    <message>
        
        <location filename="../qml/app/AppWindow.qml" line="100" />
        <source>Catalog</source>
        </message>
    <message>
        
        <location filename="../qml/app/AppWindow.qml" line="104" />
        <source>Downloads</source>
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
        
        <location filename="../qml/app/CatalogPage.qml" line="177" />
        <source>Sort</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="230" />
        <source>Filter by type</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="239" />
        <source>All</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="253" />
        <source>Portable</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="267" />
        <source>Non-portable</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="294" />
        
        <location filename="../qml/app/CatalogPage.qml" line="520" />
        <source>Catalog</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="529" />
        <source>Loading…</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="530" />
        <source>Found: %1</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="611" />
        <source>Select sources</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="612" />
        <source>Nothing found</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="620" />
        <source>Try another search or refresh the catalog.</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="628" />
        <source>Refresh</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="661" />
        <source>No games</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="679" />
        <source>Add catalog</source>
        </message>
    <message>
        
        <location filename="../qml/app/CatalogPage.qml" line="686" />
        <source>Settings</source>
        </message>

</context>
<context>
    <name>CatalogScrollHeader</name>
    <message>
        
        <location filename="../qml/components/CatalogScrollHeader.qml" line="35" />
        <source>Loading catalog…</source>
        </message>
    <message>
        
        <location filename="../qml/components/CatalogScrollHeader.qml" line="36" />
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
    <name>Core</name>
    <message>
        
        <location filename="../src/core/catalog_types.cpp" line="13" />
        <source>Game</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog_types.cpp" line="17" />
        <source>Add-on</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog_types.cpp" line="19" />
        <source>Component</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog_types.cpp" line="26" />
        <source>Direct</source>
        </message>
    <message>
        
        <location filename="../src/core/catalog_types.cpp" line="28" />
        <source>Torrent</source>
        </message>
    <message>
        
        <location filename="../src/core/job_kind.cpp" line="11" />
        
        <location filename="../src/core/catalog_types.cpp" line="30" />
        <source>Download</source>
        </message>
    <message>
        
        <location filename="../src/core/install_kind.cpp" line="11" />
        <source>Portable</source>
        </message>
    <message>
        
        <location filename="../src/core/install_kind.cpp" line="13" />
        <source>Installer</source>
        </message>
    <message>
        
        <location filename="../src/core/install_kind.cpp" line="15" />
        <source>Bundled fix</source>
        </message>
    <message>
        
        <location filename="../src/core/install_kind.cpp" line="17" />
        <source>Separate fix</source>
        </message>
    <message>
        
        <location filename="../src/core/install_kind.cpp" line="19" />
        <source>Unknown</source>
        </message>
    <message>
        
        <location filename="../src/core/job_display.cpp" line="45" />
        
        <location filename="../src/core/job_display.cpp" line="50" />
        <source>Add-on %1 — %2</source>
        </message>
    <message>
        
        <location filename="../src/core/job_display.cpp" line="99" />
        
        <location filename="../src/core/job_display.cpp" line="101" />
        <source>Install failed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/job_display.cpp" line="104" />
        
        <location filename="../src/core/job_display.cpp" line="108" />
        <source>Installing (%1/%2)</source>
        </message>
    <message>
        
        <location filename="../src/core/job_display.cpp" line="113" />
        
        <location filename="../src/core/job_display.cpp" line="115" />
        <source>Error: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/job_kind.cpp" line="13" />
        <source>Install</source>
        </message>
    <message>
        
        <location filename="../src/core/job_kind.cpp" line="15" />
        <source>Update</source>
        </message>
    <message>
        
        <location filename="../src/core/job_kind.cpp" line="17" />
        <source>Task</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="10" />
        <source>Queued</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="12" />
        <source>Starting</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="14" />
        <source>Checking</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="16" />
        <source>Metadata</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="18" />
        <source>Downloading</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="20" />
        <source>Installing</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="22" />
        <source>Seeding</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="24" />
        <source>Paused</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="26" />
        <source>Completed</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="28" />
        <source>Failed</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="30" />
        <source>Cancelled</source>
        </message>
    <message>
        
        <location filename="../src/core/job_status.cpp" line="37" />
        <source>Install failed</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="238" />
        <source>Catalog error: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="339" />
        <source>Game not found for add-on</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="344" />
        <source>Add-on not found in catalog</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="355" />
        <source>Could not find game to install: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="391" />
        <source>Download error: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="435" />
        <source>Installation of %1 is already in progress</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="641" />
        <source>Source plugin not found: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="495" />
        <source>Install failed for %1: %2</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="531" />
        <source>Update installed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="533" />
        <source>Installed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="629" />
        <source>Add-on installation is already in progress</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="635" />
        <source>Install the game first</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="698" />
        <source>Add-on install failed for %1: %2</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="719" />
        <source>Add-on installed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="868" />
        
        <location filename="../src/core/core_controller.cpp" line="3062" />
        <source>Game not found</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="874" />
        
        <location filename="../src/core/core_controller.cpp" line="3137" />
        <source>Add-on not found</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="880" />
        <source>Download the add-on first</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="1564" />
        
        <location filename="../src/core/core_controller.cpp" line="1577" />
        <source>%1 update(s) available</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="1606" />
        <source>Started %1 update(s)</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="1913" />
        <source>Install Proton-GE in Settings → Launch before downloading games</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="1916" />
        <source>Install %1 (Proton-GE) in Settings → Launch before downloading games</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2983" />
        <source>Choose library folder</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3424" />
        <source>Choose game install folder</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3453" />
        <source>No game executable found in %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3469" />
        <source>Installed</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3470" />
        <source>Manual install complete for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3589" />
        <source>Install plugin</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3591" />
        <source>Plugin package (*.arach)</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="1566" />
        <source>No updates</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="199" />
        <source>Proton-GE installed</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="201" />
        <source>Proton-GE download failed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="385" />
        <source>Download complete — install manually</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="462" />
        <source>No install handler for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="1751" />
        <source>Choose game executable</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="1754" />
        <source>Executables (*.exe *.sh *.x86_64);;All files (*)</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2363" />
        <source>%1 · %2 games</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2365" />
        <source>%1 sources · %2 games</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2456" />
        <source>Catalog empty or unavailable: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2472" />
        <source>No catalog URL configured for source %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2568" />
        
        <location filename="../src/core/core_controller.cpp" line="3131" />
        <source>Game not found: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2572" />
        <source>%1 is not installed yet</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2577" />
        <source>%1 is already running</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2600" />
        <source>Proton not found. Install Proton-GE in Settings → Launch.</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2607" />
        <source>Executable not found for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2617" />
        <source>Failed to launch game</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2633" />
        <source>Failed to stop game</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2658" />
        <source>Unknown source: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2662" />
        <source>Source "%1" is disabled in settings</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2881" />
        <source>Enter a catalog URL</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2889" />
        <source>Invalid URL — http or https required</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2908" />
        <source>Catalog entry not found: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2915" />
        <source>No magnet link for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2924" />
        <source>Could not start download for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="2992" />
        <source>Game not found in library</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3017" />
        <source>Game removed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3048" />
        <source>No destination library selected</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3069" />
        <source>Game is already on this library</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3079" />
        
        <location filename="../src/core/core_controller.cpp" line="3085" />
        <source>Could not move: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3101" />
        <source>Game moved: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3143" />
        <source>Could not start add-on download</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3152" />
        <source>Entry not found: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3158" />
        <source>Could not start update for %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3168" />
        <source>No catalog sources enabled</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3236" />
        <source>Download not found</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3240" />
        <source>Installation is only available for completed downloads</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3280" />
        <source>Add-on file not found</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3292" />
        
        <location filename="../src/core/core_controller.cpp" line="3443" />
        <source>Download files not found</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3298" />
        
        <location filename="../src/core/core_controller.cpp" line="3460" />
        <source>Could not find game to install</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3431" />
        <source>Automatic install is unavailable. Run setup.exe from the download folder, then use the folder button to point to the game.</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3544" />
        <source>Plugin installed</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3547" />
        <source>Plugin install failed: %1</source>
        </message>
    <message>
        
        <location filename="../src/core/core_controller.cpp" line="3602" />
        <source>Could not open plugins folder</source>
        </message>
    <message>
        
        <location filename="../src/core/torrent_session.cpp" line="50" />
        
        <location filename="../src/core/torrent_session.cpp" line="56" />
        <source>Torrent error %1</source>
        </message>
    <message>
        
        <location filename="../src/core/torrent_session.cpp" line="243" />
        <source>No magnet link</source>
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
    <name>DownloadJobGroupCard</name>
    <message>
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="66" />
        <source>%1 add-ons · %2 downloading</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="68" />
        <source>%1 add-ons · done</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="69" />
        <source>%1 add-ons</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="191" />
        <source>Add-ons</source>
        </message>

</context>
<context>
    <name>DownloadProgressButton</name>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="17" />
        <source>Download torrent</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="40" />
        <source>Installing…</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="42" />
        <source>Retry install</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="44" />
        <source>Install</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="46" />
        <source>Downloaded</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="48" />
        <source>Paused · %1%</source>
        </message>
    <message>
        
        <location filename="../qml/components/DownloadProgressButton.qml" line="50" />
        <source>Downloading · %1%</source>
        </message>

</context>
<context>
    <name>DownloadsPage</name>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="79" />
        <source>No downloads</source>
        </message>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="111" />
        <source>Downloads</source>
        </message>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="117" />
        <source>%1 active · resume after restart</source>
        </message>
    <message>
        
        <location filename="../qml/app/DownloadsPage.qml" line="118" />
        <source>Torrents resume after restart</source>
        </message>

</context>
<context>
    <name>GameDetailsMediaSection</name>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaSection.qml" line="316" />
        <source>Gameplay video</source>
        </message>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaSection.qml" line="88" />
        
        <location filename="../qml/components/GameDetailsMediaSection.qml" line="497" />
        <source>Screenshots</source>
        </message>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaSection.qml" line="494" />
        <source>Screenshot %1 of %2</source>
        </message>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaSection.qml" line="430" />
        
        <location filename="../qml/components/GameDetailsMediaSection.qml" line="603" />
        <source>Close</source>
        </message>
    <message>
        
        <location filename="../qml/components/GameDetailsMediaSection.qml" line="448" />
        
        <location filename="../qml/components/GameDetailsMediaSection.qml" line="612" />
        <source>Open in browser</source>
        </message>

</context>
<context>
    <name>GameDetailsPage</name>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="234" />
        <source>Game details</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="261" />
        <source>Game not found</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="294" />
        <source>%1 add-ons</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="303" />
        <source>Update available</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="319" />
        <source>Source page</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="320" />
        <source>Source website</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="331" />
        <source>Steam</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="350" />
        <source>Install failed</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="361" />
        <source>Stop</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="361" />
        <source>Play</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="398" />
        
        <location filename="../qml/app/GameDetailsPage.qml" line="528" />
        <source>Delete</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="415" />
        <source>Update</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="477" />
        <source>Description</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="483" />
        <source>Description is not available yet.</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="500" />
        <source>Remove game?</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameDetailsPage.qml" line="522" />
        <source>Cancel</source>
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
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="158" />
        <source>Proton</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="164" />
        <source>Override Proton for this game. Default uses Settings → Launch.</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="175" />
        <source>Default</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="214" />
        <source>Launch options</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="221" />
        <source>Extra launch arguments for this game</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="232" />
        <source>Custom executable (optional)</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="269" />
        <source>Information</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="275" />
        <source>Source</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="276" />
        <source>Version</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="277" />
        <source>Size</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="278" />
        <source>Install type</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="280" />
        <source>Install path</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="284" />
        <source>Installing…</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="286" />
        <source>Waiting to install</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="287" />
        <source>—</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="290" />
        <source>Download</source>
        </message>
    <message>
        
        <location filename="../qml/app/GameSettingsSheet.qml" line="324" />
        <source>Done</source>
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
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="170" />
        <source>Nothing here yet</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="190" />
        <source>Open catalog</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="191" />
        <source>Install plugin</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="206" />
        <source>Settings</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="214" />
        <source>Catalogs and plugins</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="230" />
        <source>Step 1</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="231" />
        <source>Plugin</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="236" />
        <source>Step 2</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="237" />
        <source>Catalog</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="242" />
        <source>Step 3</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="243" />
        <source>Library</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="440" />
        <source>Running</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="479" />
        <source>Play</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="488" />
        <source>Details</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="496" />
        <source>Update</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="534" />
        <source>In library</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="541" />
        <source>Sources</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="548" />
        <source>Tasks</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="555" />
        <source>Updates</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="587" />
        <source>%1 active downloads</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="594" />
        <source>Downloads continue after restart</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="601" />
        <source>Open</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="618" />
        <source>My library</source>
        </message>
    <message>
        
        <location filename="../qml/app/LibraryPage.qml" line="623" />
        <source>%1 games</source>
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
        
        <location filename="../qml/components/RunningGameBar.qml" line="67" />
        <source>Playing now</source>
        </message>
    <message>
        
        <location filename="../qml/components/RunningGameBar.qml" line="84" />
        <source>Stop</source>
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
        <source>Material 3 theme and palette apply across the app.</source>
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
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="68" />
        <source>Choose a section — each screen covers part of your setup.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="18" />
        <source>Plugins</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="19" />
        <source>FreeTP and others — install, launch, add-ons (.arach)</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="23" />
        <source>Hydra catalogs</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="24" />
        <source>games.json by URL — migrate from Hydra Launcher</source>
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
        <source>Auto-check updates and portable integrity</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="38" />
        <source>Launch</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="39" />
        <source>Global arguments and Proton-GE on Linux</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="43" />
        <source>Appearance</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="44" />
        <source>Theme, palette, accent color, and language</source>
        </message>

</context>
<context>
    <name>SettingsLaunchPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="38" />
        <source>Extra command-line arguments appended to every game launch.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="49" />
        <source>Global launch arguments</source>
        </message>
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
        
        <location filename="../qml/settings/SettingsLaunchPage.qml" line="123" />
        <source>Download Proton-GE</source>
        </message>

</context>
<context>
    <name>SettingsPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="25" />
        
        <location filename="../qml/settings/SettingsPage.qml" line="28" />
        
        <location filename="../qml/settings/SettingsPage.qml" line="123" />
        <source>Settings</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="132" />
        <source>Plugins</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="140" />
        <source>Hydra catalogs</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="152" />
        <source>Edit catalog</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="152" />
        <source>New Hydra catalog</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="162" />
        <source>Storage</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="170" />
        <source>Updates</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="178" />
        <source>Launch</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="186" />
        <source>Appearance</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="240" />
        <source>Back</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPage.qml" line="253" />
        <source>Done</source>
        </message>

</context>
<context>
    <name>SettingsPluginsPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="71" />
        <source>No plugins found</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="147" />
        <source>v%1 · %2</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="187" />
        <source>Install .arach…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="198" />
        <source>Open folder</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="205" />
        <source>Refresh</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsPluginsPage.qml" line="215" />
        <source>User-installed: %1</source>
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
        <source>URL games.json</source>
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
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="176" />
        <source>Add drive…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="194" />
        <source>Games: %1</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="281" />
        <source>No games on this drive yet</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="299" />
        <source>Delete</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="313" />
        <source>Move…</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsStoragePage.qml" line="332" />
        <source>Move to drive</source>
        </message>

</context>
<context>
    <name>SettingsUpdatesPage</name>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="28" />
        <source>Update checks and automatic installs.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="46" />
        <source>Check for updates when loading the catalog</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="52" />
        <source>Compares build dates in the catalog with installed games.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="77" />
        <source>Install updates automatically on launch</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="83" />
        <source>Starts downloads for games with updates when the catalog finishes loading. Per-game opt-out is available in game details.</source>
        </message>
    <message>
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="103" />
        <source>Check for updates</source>
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
</TS>
