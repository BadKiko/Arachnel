# Releases (GitHub Actions)

Manual release workflow: **Actions → Release → Run workflow**.

Inputs:

| Field | Meaning |
|-------|---------|
| `version` | Semver without `v`, e.g. `0.1.39` → tag `v0.1.39`. Not `dev`. |
| `prerelease` | Pre-release flag on GitHub |

Artifacts:

- `Arachnel-<version>-Setup.exe` — Windows installer (Inno Setup + app from dist-win)
- `Arachnel-<version>-x86_64.AppImage` — Linux portable build
- `Arachnel-Setup.exe` / `Arachnel-x86_64.AppImage` — stable aliases (always latest of that release; used by VirusTotal badge)
- `checksums.sha256` — SHA-256 for the versioned files

The release notes body lists commits since the previous `v*` tag.

## Versioning

| Context | Version source |
|---------|----------------|
| Local `.\run.ps1` (dev run) | `dev` unless `ARACHNEL_VERSION` is set |
| `.\run.ps1 --package` / `--installer` | `ARACHNEL_VERSION`, else nearest `v*` git tag (`git describe`). Refuses bare `dev` for `--installer` |
| CI Release | workflow input `version` (validated, not `dev`) |

The app shows whatever was baked in at CMake configure time (`ARACHNEL_VERSION`).

## Upgrade / player data safety

- **Game libraries and settings** live under AppData / user library folders, not inside the launcher install dir. Updating the launcher only replaces files under `{app}` (binaries, Qt, etc.).
- **In-app update** downloads `Arachnel-*-Setup.exe` and runs Inno with `/SILENT /DIR=<install dir>` (unpack progress, no folder prompts, then auto-launches Arachnel). Legacy Program Files installs also pass `/ALLUSERS` so elevation can work.
- **Legacy installs** (`C:\Program Files\Arachnel`, uninstall key `...\Uninstall\Arachnel`): the Inno installer detects that `InstallLocation`, offers it as the default folder, writes the new Inno uninstall entry, removes the old uninstall registry key, and deletes leftover `uninstall.exe` / setup stubs under `{app}`. It does **not** run the old uninstaller.
- New default for fresh installs: `%LOCALAPPDATA%\Programs\Arachnel` (`PrivilegesRequired=lowest`, elevates only when the target needs it).

## CI speed (caching)

Release builds already cache:

| Cache | What |
|-------|------|
| Qt (`install-qt-action`) | Qt kits |
| **sccache** | Compiled objects (Arachnel, libtorrent, QmlMaterial, QML cachegen C++) |
| **FetchContent** (`.cache/fetchcontent`) | Boost headers, libtorrent + QmlMaterial sources |
| linuxdeploy tools | AppImage packagers (Linux) |

First release after a toolchain/dep bump is still cold; later runs reuse hits. Check the **sccache stats** step in the Actions log.

## Windows code signing (required for public releases)

Without secrets the EXE is **unsigned**. Windows Defender ML and SmartScreen often flag unsigned installers from unknown publishers.

Add repository secrets:

| Secret | Value |
|--------|--------|
| `WINDOWS_SIGN_CERT_PFX_BASE64` | Base64 of your `.pfx` code-signing certificate |
| `WINDOWS_SIGN_CERT_PASSWORD` | PFX password |
| `WINDOWS_SIGN_TIMESTAMP_URL` | Optional; default `http://timestamp.digicert.com` |

Export PFX (PowerShell):

```powershell
[Convert]::ToBase64String([IO.File]::ReadAllBytes("C:\path\cert.pfx")) | Set-Clipboard
```

You need an **Authenticode** certificate (OV/EV from a public CA). Self-signed certs do not remove SmartScreen for unknown publishers.

When secrets are set, `.\run.ps1 --installer` (via `setup\inno\pack-inno.ps1`) signs the final Inno `Arachnel-Setup.exe`.

## VirusTotal (release binaries)

CI uploads each release `Setup.exe` + AppImage to VirusTotal and refreshes the README badge.

| Secret | Value |
|--------|--------|
| `VT_API_KEY` | API key from [VirusTotal → API key](https://www.virustotal.com/gui/my-apikey) |

Workflow: **Actions → VirusTotal** (also runs on every published release + weekly).

Notes:

- Scans **release binaries only** (what users download). Scanning every source file burns the free API quota and does not help SmartScreen.
- SmartScreen / Defender “unknown publisher” is mostly about **code signing**, not VirusTotal. VT is transparency for multi-engine results.
- Release notes get a VirusTotal section with per-file links. Stable aliases `Arachnel-Setup.exe` / `Arachnel-x86_64.AppImage` are uploaded alongside versioned assets.

### Defender false positives (`Sabsik.TE.A!ml`)

`Trojan:Win32/Sabsik.TE.A!ml` is a **Microsoft ML heuristic** (`!ml`), not a known malware signature. It often fires on unsigned installers from unknown publishers.

Shipping Windows builds use **Inno Setup**. That is usually quieter than a hand-rolled dropper, but **OV/EV signing is still required** for sane public releases. Without a cert, Defender/SmartScreen can still warn on new hashes.

If a signed build is still quarantined:

1. Prefer distributing via **GitHub Releases HTTPS**, not Telegram Desktop downloads (Telegram paths get hit harder)
2. Submit a false positive at [Microsoft WDSI file submission](https://www.microsoft.com/wdsi/filesubmission) → Software developer → false positive, attach the Setup hash

## Local dry run

```powershell
# Windows - set a real version (or have a v* git tag)
$env:ARACHNEL_VERSION = "0.1.39"
$env:BUILD_TYPE = "Release"
$env:ARACHNEL_FAST_BUILD = "0"
.\run.ps1 --installer
```

```bash
# Linux AppImage
export ARACHNEL_VERSION=0.1.39
export CMAKE_PREFIX_PATH=/path/to/Qt/6.8/gcc_64
bash scripts/ci/package-appimage.sh
```
