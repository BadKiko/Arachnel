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

After building, FreeTP is in dist:
build-win/dist/freetp.arach</source>
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
        
        
        <location filename="../qml/app/AppWindow.qml" line="82" />
        <source>Library</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/AppWindow.qml" line="86" />
        <source>Catalog</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/AppWindow.qml" line="90" />
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
        
        
        <location filename="../qml/app/CatalogPage.qml" line="136" />
        <source>Sort</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/CatalogPage.qml" line="177" />
        
        
        <location filename="../qml/app/CatalogPage.qml" line="401" />
        <source>Catalog</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/CatalogPage.qml" line="410" />
        <source>Loading…</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/CatalogPage.qml" line="411" />
        <source>Found: %1</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/CatalogPage.qml" line="492" />
        <source>Select sources</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/CatalogPage.qml" line="493" />
        <source>Nothing found</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/CatalogPage.qml" line="501" />
        <source>Try another search or refresh the catalog.</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/CatalogPage.qml" line="509" />
        <source>Refresh</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/CatalogPage.qml" line="542" />
        <source>No games</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/CatalogPage.qml" line="560" />
        <source>Add catalog</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/CatalogPage.qml" line="567" />
        <source>Settings</source>
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
        
        
        <location filename="../src/core/catalog_types.cpp" line="30" />
        
        
        <location filename="../src/core/job_kind.cpp" line="11" />
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
        
        
        <location filename="../src/core/core_controller.cpp" line="151" />
        <source>Catalog error: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="257" />
        <source>Game not found for add-on</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="262" />
        <source>Add-on not found in catalog</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="273" />
        <source>Could not find game to install: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="320" />
        <source>Download error: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="364" />
        <source>Installation of %1 is already in progress</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="370" />
        
        
        <location filename="../src/core/core_controller.cpp" line="578" />
        <source>Source plugin not found: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="412" />
        <source>Install failed for %1: %2</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="469" />
        <source>Update installed: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="471" />
        <source>Installed: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="566" />
        <source>Add-on installation is already in progress</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="572" />
        <source>Install the game first</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="632" />
        <source>Add-on install failed for %1: %2</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="653" />
        <source>Add-on installed: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="734" />
        
        
        <location filename="../src/core/core_controller.cpp" line="1314" />
        
        
        <location filename="../src/core/core_controller.cpp" line="2294" />
        <source>Game not found</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="740" />
        
        
        <location filename="../src/core/core_controller.cpp" line="2369" />
        <source>Add-on not found</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="746" />
        <source>Download the add-on first</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1293" />
        
        
        <location filename="../src/core/core_controller.cpp" line="1307" />
        <source>%1 update(s) available</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1295" />
        <source>No updates</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1316" />
        <source>Game not installed</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1318" />
        <source>Install folder not found</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1325" />
        <source>Executable not found</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1327" />
        <source>Executable is missing</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1649" />
        <source>%1 · %2 games</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1651" />
        <source>%1 sources · %2 games</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1718" />
        <source>Catalog empty or unavailable: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1736" />
        <source>No catalog URL configured for source %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1823" />
        
        
        <location filename="../src/core/core_controller.cpp" line="2363" />
        <source>Game not found: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1827" />
        <source>%1 is not installed yet</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1841" />
        <source>%1 is already running</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1850" />
        <source>Executable not found for %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1860" />
        <source>Failed to launch game</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1876" />
        <source>Failed to stop game</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1901" />
        <source>Unknown source: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="1905" />
        <source>Source "%1" is disabled in settings</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2124" />
        <source>Enter a catalog URL</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2132" />
        <source>Invalid URL — http or https required</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2144" />
        <source>Catalog entry not found: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2149" />
        <source>No magnet link for %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2158" />
        <source>Could not start download for %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2215" />
        <source>Folder picker is only available on Windows</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2224" />
        <source>Game not found in library</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2249" />
        <source>Game removed: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2280" />
        <source>No destination library selected</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2301" />
        <source>Game is already on this library</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2311" />
        
        
        <location filename="../src/core/core_controller.cpp" line="2317" />
        <source>Could not move: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2333" />
        <source>Game moved: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2375" />
        <source>Could not start add-on download</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2384" />
        <source>Entry not found: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2390" />
        <source>Could not start update for %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2400" />
        <source>No catalog sources enabled</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2416" />
        <source>Files OK: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2434" />
        <source>No installed portable games to verify</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2439" />
        <source>Verified %1 portable game(s) — all OK</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2441" />
        <source>Verified: %1, issues: %2</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2502" />
        <source>Download not found</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2506" />
        <source>Installation is only available for completed downloads</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2538" />
        <source>Add-on file not found</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2543" />
        <source>Download files not found</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2547" />
        <source>Source plugin not found</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2553" />
        <source>Could not find game to install</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2662" />
        <source>Plugin installed</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2665" />
        <source>Plugin install failed: %1</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2705" />
        <source>File picker is only available on Windows</source>
        </message>
    <message>
        
        
        <location filename="../src/core/core_controller.cpp" line="2714" />
        <source>Could not open plugins folder</source>
        </message>


</context>
<context>
    <name>DownloadJobGroupCard</name>
    <message>
        
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="65" />
        <source>%1 add-ons · %2 downloading</source>
        </message>
    <message>
        
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="67" />
        <source>%1 add-ons · done</source>
        </message>
    <message>
        
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="68" />
        <source>%1 add-ons</source>
        </message>
    <message>
        
        
        <location filename="../qml/components/DownloadJobGroupCard.qml" line="182" />
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
    <name>GameDetailsPage</name>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="140" />
        <source>Game details</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="167" />
        <source>Game not found</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="200" />
        <source>%1 add-ons</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="209" />
        <source>Update available</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="226" />
        <source>Install failed</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="237" />
        <source>Stop</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="237" />
        <source>Play</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="273" />
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="440" />
        <source>Delete</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="281" />
        <source>Refresh</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="290" />
        <source>Verify files</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="327" />
        <source>Description</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="333" />
        <source>Description is not available yet.</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="359" />
        <source>Information</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="365" />
        <source>Source</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="366" />
        <source>Version</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="367" />
        <source>Size</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="368" />
        <source>Install type</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="370" />
        <source>Install path</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="374" />
        <source>Installing…</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="376" />
        <source>Waiting to install</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="377" />
        <source>—</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="380" />
        <source>Download</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="412" />
        <source>Remove game?</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/GameDetailsPage.qml" line="434" />
        <source>Cancel</source>
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
        
        
        <location filename="../qml/settings/InstallLocationSheet.qml" line="34" />
        
        
        <location filename="../qml/settings/InstallLocationSheet.qml" line="148" />
        <source>Install</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/InstallLocationSheet.qml" line="42" />
        <source>Choose a drive for installation</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/InstallLocationSheet.qml" line="52" />
        <source>Install to:</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/InstallLocationSheet.qml" line="141" />
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
        
        
        <location filename="../qml/components/LibraryGameCard.qml" line="51" />
        <source>Installing %1%</source>
        </message>
    <message>
        
        
        <location filename="../qml/components/LibraryGameCard.qml" line="54" />
        <source>Installing…</source>
        </message>
    <message>
        
        
        <location filename="../qml/components/LibraryGameCard.qml" line="56" />
        <source>Paused · %1%</source>
        </message>
    <message>
        
        
        <location filename="../qml/components/LibraryGameCard.qml" line="57" />
        <source>Downloading %1%</source>
        </message>
    <message>
        
        
        <location filename="../qml/components/LibraryGameCard.qml" line="115" />
        <source>Playing</source>
        </message>
    <message>
        
        
        <location filename="../qml/components/LibraryGameCard.qml" line="134" />
        <source>Updating</source>
        </message>
    <message>
        
        
        <location filename="../qml/components/LibraryGameCard.qml" line="186" />
        <source>Running</source>
        </message>


</context>
<context>
    <name>LibraryPage</name>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="29" />
        <source>Playing now</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="30" />
        <source>Recently played</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="60" />
        <source>Installing %1%</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="63" />
        <source>Installing…</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="65" />
        <source>Paused · %1%</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="66" />
        <source>Downloading %1%</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="155" />
        <source>Nothing here yet</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="175" />
        <source>Open catalog</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="176" />
        <source>Install plugin</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="191" />
        <source>Settings</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="199" />
        <source>Catalogs and plugins</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="215" />
        <source>Step 1</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="216" />
        <source>Plugin</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="221" />
        <source>Step 2</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="222" />
        <source>Catalog</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="227" />
        <source>Step 3</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="228" />
        <source>Library</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="422" />
        <source>Running</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="461" />
        <source>Play</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="469" />
        <source>Details</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="477" />
        <source>Refresh</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="514" />
        <source>In library</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="521" />
        <source>Sources</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="528" />
        <source>Tasks</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="535" />
        <source>Updates</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="567" />
        <source>%1 active downloads</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="574" />
        <source>Downloads continue after restart</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="581" />
        <source>Open</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="594" />
        <source>My library</source>
        </message>
    <message>
        
        
        <location filename="../qml/app/LibraryPage.qml" line="599" />
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
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="20" />
        <source>Choose a section — each screen covers part of your setup.</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="37" />
        <source>Plugins</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="38" />
        <source>FreeTP and others — install, launch, add-ons (.arach)</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="42" />
        <source>Hydra catalogs</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="43" />
        <source>games.json by URL — migrate from Hydra Launcher</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="47" />
        <source>Storage</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="48" />
        <source>Library and download folders</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="52" />
        <source>Updates</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="53" />
        <source>Auto-check updates and portable integrity</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="57" />
        <source>Appearance</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsHubPage.qml" line="58" />
        <source>Theme, palette, accent color, and language</source>
        </message>


</context>
<context>
    <name>SettingsPage</name>
    <message>
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="23" />
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="26" />
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="117" />
        <source>Settings</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="126" />
        <source>Plugins</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="134" />
        <source>Hydra catalogs</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="146" />
        <source>Edit catalog</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="146" />
        <source>New Hydra catalog</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="156" />
        <source>Storage</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="164" />
        <source>Updates</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="172" />
        <source>Appearance</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="226" />
        <source>Back</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsPage.qml" line="239" />
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
        <source>Update checks and portable build integrity verification.</source>
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
        <source>Verify portable files before launch</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="83" />
        <source>The install folder and .exe must exist.</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="103" />
        <source>Check for updates</source>
        </message>
    <message>
        
        
        <location filename="../qml/settings/SettingsUpdatesPage.qml" line="110" />
        <source>Verify portable</source>
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
