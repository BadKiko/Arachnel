# Linux fix launch assets (optional)

Place stub DLLs from [onlinefix-linux](https://github.com/ZzEdovec/onlinefix-linux) release here for **fake Steam** mode when Steam is not running:

- `ftpPath32.dll` — replaces `steamfix32.dll`
- `ftpPath64.dll` — replaces `steamfix64.dll`
- `Newtonsoft.Json.dll` — Photon Launcher dependency (optional if already in game)

Without `ftpPath*.dll`, Arachnel still applies `WINEDLLOVERRIDES` and FreeTP INI patches; only the no-Steam DLL swap is skipped.
