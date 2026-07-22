# Releases (GitHub Actions)

Manual release workflow: **Actions → Release → Run workflow**.

Inputs:

| Field | Meaning |
|-------|---------|
| `version` | Semver without `v`, e.g. `0.1.0` → tag `v0.1.0` |
| `prerelease` | Pre-release flag on GitHub |

Artifacts:

- `Arachnel-<version>-Setup.exe` — Windows installer (setup + embedded app)
- `Arachnel-<version>-x86_64.AppImage` — Linux portable build
- `checksums.sha256` — SHA-256 for both files

The release notes body lists commits since the previous `v*` tag.

## CI speed (caching)

Release builds already cache:

| Cache | What |
|-------|------|
| Qt (`install-qt-action`) | Qt kits |
| **sccache** | Compiled objects (Arachnel, libtorrent, QmlMaterial, QML cachegen C++) |
| **FetchContent** (`.cache/fetchcontent`) | Boost headers, libtorrent + QmlMaterial sources |
| linuxdeploy tools | AppImage packagers (Linux) |

First release after a toolchain/dep bump is still cold; later runs reuse hits. Check the **sccache stats** step in the Actions log.

## Windows code signing (optional)

Without secrets the EXE is **unsigned** (SmartScreen may warn).

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

## Local dry run

```powershell
# Windows
$env:BUILD_TYPE = "Release"
$env:ARACHNEL_FAST_BUILD = "0"
.\run.ps1 --installer
```

```bash
# Linux AppImage
export ARACHNEL_VERSION=0.1.0-dev
export CMAKE_PREFIX_PATH=/path/to/Qt/6.8/gcc_64
bash scripts/ci/package-appimage.sh
```
