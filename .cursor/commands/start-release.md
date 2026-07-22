# Start release

Run the **full Arachnel ecosystem release** now: sync git → start home GitLab Runner → ship Arachnel + plugins via CI → update sourcelist. Watch every stage; on failure diagnose, fix, re-run that stage.

This command **does** commit/push/tag/release as needed (unlike a dry local build).

## Paths

| Repo | Path | Remote | Default branch |
|------|------|--------|----------------|
| Arachnel | `D:\Work\Arachnel` | `origin` (GitHub) | `master` |
| Steam (steamidra) | `D:\PetWork\arachnel-plugin-steamidra` | `origin` (GitLab) | `master` |
| FreeTP | `D:\PetWork\arachnel-plugin-freetp` | `BadKiko` (GitLab) | `main` |
| Sourcelist | `D:\PetWork\arachnel-plugins-sourcelist` | `origin` (GitLab) | `main` |

Home Windows runner: `C:\GitLab-Runner\` (tags `windows`, `pc`). Start script: `C:\GitLab-Runner\start-runner.ps1`.  
Linux jobs: VPS runners tagged `vps` (must already be online).

Auth: use existing `gh` login and `$env:GITLAB_TOKEN` / `glab` if configured. **Never paste tokens into chat or commit them.**

## Checklist

```
- [ ] 1. Sync every repo onto default branch; commit release-ready work; push
- [ ] 2. Start home GitLab Runner; confirm online
- [ ] 3. Decide versions (Arachnel + Steam + FreeTP)
- [ ] 4. Trigger Arachnel GitHub Release + plugin tag pipelines (parallel)
- [ ] 5. Monitor CI; fix failures; re-tag / re-dispatch if needed
- [ ] 6. Update sourcelist from universal GitLab packages; push main
- [ ] 7. Report URLs (releases, pipelines, sourcelist)
```

---

## Step 1 — Git sync (master/main + push)

For **each** of Arachnel, steamidra, freetp, sourcelist:

1. `git fetch` the correct remote.
2. If not on default branch (`master` / `main`): checkout default **or** merge default into current then checkout default — prefer ending on default for release.
3. `git merge` / `git pull --ff-only` so local matches remote tip (resolve conflicts).
4. If there are **unpushed commits**: `git push`.
5. If there is **uncommitted release source** (plugin code, `plugin.json`, CI, launcher code — **not** `build-*`, `dist/`, `.ci/`, `aqtinstall.log`, `libs/*` noise):
   - Stage only release files
   - Commit with a clear message
   - Push to default branch

FreeTP push remote is **`BadKiko`**, not `origin`.

Do not leave release work only on a feature branch.

---

## Step 2 — Start home GitLab Runner

```powershell
pwsh -File C:\GitLab-Runner\start-runner.ps1
Get-Process gitlab-runner
& C:\GitLab-Runner\gitlab-runner.exe verify -c C:\GitLab-Runner\config.toml
```

If process missing / verify fails: fix `C:\GitLab-Runner\config.toml`, restart script, do not tag until Windows runner is up (plugin `build:windows` uses tag `windows`).

Optional: API-check runner `online=true` for steamidra/freetp projects.

---

## Step 3 — Versions

Read current versions:

- Arachnel: latest `v*` tag + any pending changes since it
- Steam: `plugin.json` → version (e.g. `0.3.30`) vs latest tag `v0.3.28`
- FreeTP: `plugin.json` vs latest tag

Rules:

- If `plugin.json` is ahead of latest tag → release that version (tag `vX.Y.Z`).
- If equal and no meaningful changes → skip that plugin (say so).
- If behind / mismatch → bump `plugin.json`, commit, push, then tag.
- Arachnel: next semver (patch unless user asked otherwise). Confirm briefly in the report if bumping.

Keep `scripts/ci/launcher-toolchain.env` `ARACHNEL_SDK_REF` aligned with the Arachnel tag you are shipping (or the last published launcher the plugins need).

---

## Step 4 — Trigger builds (parallel)

### Arachnel (GitHub Actions)

```powershell
cd D:\Work\Arachnel
# ensure master pushed
gh workflow run Release --ref master -f version=<X.Y.Z> -f prerelease=false
gh run list --workflow=Release --limit 3
```

Docs: `docs/RELEASE.md`. Artifacts: Setup.exe + AppImage.

### Steam plugin (GitLab tag)

```powershell
cd D:\PetWork\arachnel-plugin-steamidra
# master pushed with matching plugin.json
git tag -a v<X.Y.Z> -m "Steam plugin v<X.Y.Z>"
git push origin v<X.Y.Z>
```

Pipeline: `build:windows` (home PC) + `build:linux` (vps) → `package:universal` → `release` → generic package `steam.arach`.

### FreeTP (GitLab tag)

```powershell
cd D:\PetWork\arachnel-plugin-freetp
git tag -a v<X.Y.Z> -m "FreeTP plugin v<X.Y.Z>"
git push BadKiko v<X.Y.Z>
```

Same shape: windows + vps → universal `freetp.arach`.

Start Arachnel workflow and both tag pushes in the same turn when possible.

---

## Step 5 — Monitor and fix

Poll until green:

```powershell
# GitHub
gh run watch <run-id> --repo BadKiko/Arachnel

# GitLab (glab or API)
glab ci status --repo BadKiko/arachnel-plugin-steamidra
# or open pipeline URL from tag push output
```

On failure:

| Failure | Action |
|---------|--------|
| Windows job pending forever | Restart `start-runner.ps1`; confirm tag `windows` |
| Compile / SDK error | Fix on default branch, push, delete+retags or bump patch and re-tag |
| Linux/vps offline | Note blocker; do not fake sourcelist with windows-only local MinGW builds |
| Arachnel GHA fail | Fix on `master`, re-run workflow |
| Tag already exists wrong | Delete remote tag only if intentional, then re-push corrected tag |

Re-run only the failed product; keep successful siblings.

---

## Step 6 — Sourcelist (after plugin releases OK)

Prefer **universal** packages from GitLab Release/package (not local MinGW `build-win\dist`).

```powershell
$src = "D:\PetWork\arachnel-plugins-sourcelist"
cd $src
git checkout main
git pull --ff-only origin main

# Download steam.arach + freetp.arach from the new GitLab package/release asset URLs
# (from pipeline release job or packages API)

python tools/generate_plugins_index.py
git add steam.arach freetp.arach plugins.json
git commit -m "Update store plugins: Steam <ver> and FreeTP <ver>"
git push origin main
```

Verify `plugins.json` versions, sha256, and that `platforms` include both windows and linux for universal bundles.

Optional: wait for sourcelist CI verify jobs.

---

## Step 7 — Report

Return:

- Versions released (Arachnel / Steam / FreeTP)
- Pipeline / Actions URLs and status
- Sourcelist commit / `plugins.json` entries
- Anything skipped or still blocked (e.g. VPS down)

## Hard rules

1. Release artifacts for the store come from **CI universal** packages, not ad-hoc local MinGW `.arach`.
2. Home runner must be running before relying on `build:windows`.
3. Do not commit `build-*`, `dist/`, runner `config.toml`, tokens, or Boost `libs/` noise.
4. Fix-forward on default branches; do not abandon a half-tagged release without saying so.

## Out of scope unless asked

- Catalog full rebuild (`update-catalog-*`)
- Code-signing cert setup
- Non-Arachnel PetWork repos
