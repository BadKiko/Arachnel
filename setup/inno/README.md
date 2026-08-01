# Inno Setup (Arachnel)

Shipping Windows installer. Botva2 dark UI + payload from `dist-win/`.

## Build

```powershell
$env:ARACHNEL_VERSION = "0.1.39"   # required for --installer (or a v* git tag)
.\run.ps1 --package                  # if dist-win is missing
.\run.ps1 --installer                # -> Arachnel-Setup.exe at repo root
```

Or directly:

```powershell
.\setup\inno\pack-inno.ps1 -Version 0.1.39
```

Needs [Inno Setup 6](https://www.jrsoftware.org/isinfo.php) (`ISCC.exe`).

Output:

- `setup/inno/output/Arachnel-<ver>-Setup.exe`
- copy to repo root as `Arachnel-Setup.exe` (CI/signing)

Sign when `WINDOWS_SIGN_CERT_PFX_BASE64` + password are set.

## Upgrade notes

- Fresh install default: `%LOCALAPPDATA%\Programs\Arachnel`
- Existing **Inno** install: same folder (`UsePreviousAppDir`)
- Existing **legacy** installs (`Program Files\Arachnel` from older builds): detected via uninstall registry, offered as default; old uninstall key removed after install; leftover `uninstall.exe` deleted from `{app}`
- In-app updates: Inno `/SILENT /DIR=<install dir>` - progress window, no folder picker, then auto-launches Arachnel. Does not touch AppData game libraries/settings

## UI experiment only

[`Arachnel-UI.iss`](Arachnel-UI.iss) is a lightweight Botva2 mock (dummy file, no dist-win). Use for skin tweaks:

```powershell
.\setup\inno\skin\gen-skin.ps1
& $iscc .\setup\inno\Arachnel-UI.iss
```
