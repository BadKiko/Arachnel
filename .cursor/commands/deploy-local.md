# Deploy local

Build Arachnel and deploy a runnable copy to **`D:\Media\Arachnel`** so you can launch `arachnel_app.exe` from there.

## When to use

User runs `/deploy-local` (or asks to build & put Arachnel into `D:\Media\Arachnel`).

## Do this

1. Working directory: `D:\Work\Arachnel` (this repo).
2. Run the deploy script (**always build** — do not use `-SkipBuild` unless the user explicitly asks to only re-copy):

```powershell
pwsh -NoProfile -File .\scripts\deploy-media.ps1 -Fast
```

Flags:

| Flag | Meaning |
|------|---------|
| `-Fast` (default for this command) | Rebuild `RelWithDebInfo` via `run.ps1 --package`, then sync |
| *(no -Fast)* | `Release` package, then sync |
| `-SkipBuild` | **Only** if user asks — sync existing `dist-win` (may be stale!) |
| `-Dest PATH` | Override dest (default `D:\Media\Arachnel`) |

3. If `arachnel_app.exe` is running from the dest folder, the script stops it before sync.
4. After sync, verify the deployed exe is fresh (mtime ≈ now, not days old) and report:
   - Deploy path: `D:\Media\Arachnel\arachnel_app.exe`
   - Build type used
   - Deployed exe last-write time
   - Any build/robocopy error (fix and stop)

**Never** run `-SkipBuild` by default — that previously left a week-old `dist-win` in `D:\Media\Arachnel`.

Optional: after a successful deploy, offer to start the app:

```powershell
Start-Process -FilePath 'D:\Media\Arachnel\arachnel_app.exe' -WorkingDirectory 'D:\Media\Arachnel'
```

Only launch if the user asked to run it, or briefly confirm.

## Notes

- Do **not** commit, tag, or release (that is `/start-release`).
- Do **not** push.
- Script already calls `.\run.ps1 --package` (configure + build + `windeployqt` into `dist-win`).
- Qt / MinGW must already work for normal `.\run.ps1` builds.
